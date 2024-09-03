#include "music_queue.h"
#include <dpp/unicode_emoji.h>
#include <algorithm>
#include <random>
#include <chrono>

std::unordered_map<dpp::snowflake, music_queue*> music_queue::queue_map;
std::shared_mutex music_queue::map_mutex;
dpp::cache<dpp::message> music_queue::player_embed_cache;

music_queue* music_queue::getQueue(dpp::snowflake guild_id, bool create)
{
    std::shared_lock<std::shared_mutex> guard(map_mutex);

    auto res = queue_map.find(guild_id);
    music_queue* found_queue = nullptr;
    if (res == queue_map.end())
    {
        if (create)
        {
            // unlock shared
            guard.unlock();
            // obtain unshared write mutex
            std::unique_lock<std::shared_mutex> guard2(map_mutex);
            found_queue = new music_queue();
            queue_map.insert({guild_id, found_queue});
        }
    }
    else
        found_queue = res->second;

    return found_queue;
}

void music_queue::removeQueue(std::pair<dpp::discord_client&, dpp::snowflake> event)
{
    // capture the queue map mutex first
    std::shared_lock<std::shared_mutex> shared_map_guard(map_mutex);
    // retrieve music_queue from map if exists
    auto res = queue_map.find(event.second);
    // music_queue exists
    if (res != queue_map.end())
    {
        shared_map_guard.unlock();
        std::unique_lock<std::shared_mutex> write_map_guard(map_mutex);
        music_queue* queue_to_del = res->second;
        res->second->clear_queue();
        queue_map.erase(res); // delete from map
        // remove cache msg
        removeMessage(queue_to_del->getPlayerID());
        // msg takes 60 seconds to delete so its ok to do this after removeMessge()
        dpp::message* msg = getMessage(queue_to_del->getPlayerID());
        if (msg)
            event.first.creator->message_delete(msg->id, msg->channel_id);

        delete queue_to_del; // delete music_queue
    }
    event.first.disconnect_voice(event.second);
}

void music_queue::cacheMessage(dpp::message &msg)
{
    dpp::message* cached_msg = new dpp::message();
    *cached_msg = msg;
    player_embed_cache.store(cached_msg);
}

void music_queue::removeMessage(dpp::snowflake msg_id)
{
    dpp::message* msg = player_embed_cache.find(msg_id);
    if (msg)
        player_embed_cache.remove(msg);
}

void music_queue::updateMessage(std::pair<dpp::cluster&, dpp::snowflake> event)
{
    music_queue* queue = getQueue(event.second);
    if (queue)
    {
        dpp::message* msg = player_embed_cache.find(queue->getPlayerID());
        if (msg)
        {
            std::cout << "Updating embed\n";
            // get new embed
            dpp::message new_msg = queue->get_embed();
            // copy components and embed over
            msg->set_content("");
            msg->embeds = new_msg.embeds;
            msg->components = new_msg.components;
            event.first.message_edit(*msg);
        }
    }
}

void music_queue::setVoiceClient(dpp::discord_voice_client *voice)
{
    // acquire mutex
    std::unique_lock<std::mutex> guard(queue_mutex);
    vc = voice;
    // unlock mutex
    guard.unlock();
    // notify waiting threads that vc is ready
    vc_ready.notify_all();
    std::cout << "Notified\n";
}

/*If first song: Download and send song to discord, then add to queue
If second song, will download locally to resources/next.pcm, then add to queue
Else, will just add to queue. No download is performed yet.
Returns true if download is succesful AND song is added to queue, else false*/
bool music_queue::enqueue(song &song_to_add)
{
    // when the first song is a livestream 
    std::lock_guard<std::mutex> guard(queue_mutex); // acquire mutex for queue

    // add song to queue
    queue.push_back(song_to_add);
    // stream song or pre-download
    if (queue.size() == 1)
    {
        // stream song straight to disc
        stopLivestream = false;
        std::thread t([url = song_to_add.url, this](){ this->handle_download(url); });
        t.detach();
    }
    else if (queue.size() == 2 && song_to_add.type != livestream)
    {
        // preload
        std::thread t([url = song_to_add.url, this](){ this->preload(url); });
        t.detach();
    }
    std::cout << "Queued song\n";
    return queue.size() == 1;
}

/*Will be called when streamed song is finished. Pops top of queue
then begins streaming next song which should be located on disk at resources/next.pcm
After streaming to discord, download next song if exists to resources/next.pcm
If no track is next, return false, else true*/
bool music_queue::go_next()
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    if (!queue.empty())
    {
        queue.pop_front();

        if (!queue.empty())
        {
            // send next song
            song& next = queue.front();
            if (next.type == livestream)
            {
                stopLivestream = false;
                std::thread t([url = next.url, this](){ this->handle_download(url); });
                t.detach();
            }
            else
            {
                // read pcm bytes from file
                FILE* next_pcm = fopen(NEXT_SONG, "rb");
                if (next_pcm)
                {
                    constexpr size_t buffsize = dpp::send_audio_raw_max_length;
                    char buffer[dpp::send_audio_raw_max_length];
                    size_t curr_read = 0;

                    while (!vc->terminating && (curr_read = fread(buffer, sizeof(char), buffsize, next_pcm)) == buffsize)
                        vc->send_audio_raw((uint16_t*)buffer, buffsize);

                    if (!vc->terminating && curr_read > 0)
                    {
                        int rem = curr_read % 4;
                        vc->send_audio_raw((uint16_t*)buffer, curr_read-rem);
                    }
                }
                vc->insert_marker("end");
            }
            // preload if available
            if (queue.size() > 1 && queue.at(1).type != livestream)
            {
                std::thread t([url = queue.at(1).url, this](){ this->preload(url); });
                t.detach();
            }
            return true;
        }
    }
    return false;
}

bool music_queue::skip()
{
    if (!queue.empty() && queue.front().type == livestream)
    {
        std::lock_guard<std::mutex> guard(queue_mutex);
        // need to signal flag to end livestream. Afterwhich a marker will get added and trigger
        stopLivestream = true;
        vc->skip_to_next_marker();
        return false;
    }
    else // jus skipping a song
    {
        vc->skip_to_next_marker();
        go_next();
        return true;
    }
}

void music_queue::clear_queue()
{
    std::lock_guard<std::mutex> guard(queue_mutex);
    stopLivestream = true; // terminate livestream
    queue.clear();
    vc->stop_audio();
}

bool music_queue::remove_from_queue(size_t start, size_t end)
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    if (start < queue.size())
    {
        // sanitize end
        start += start == 0; // if start is 0 make it 1
        if (end == std::string::npos || end == queue.size())
            queue.erase(queue.begin() + start, queue.end());
        else
            queue.erase(queue.begin() + start, queue.begin() + end);

        if (start == 1 && queue.size() > 1 && queue.at(1).type != livestream)
        {
            std::thread t([url = queue.at(1).url,this](){ this->preload(url); });
            t.detach();
        }
        return true;
    }
    return false;
}

dpp::message music_queue::get_embed()
{
    dpp::message embed;
    switch (page)
    {
    case page_type::queue:
        embed = get_queue_embed();
        break;

    case page_type::history:
        embed = get_history_embed();
        break;
    
    case page_type::playback_control:
    default:
        embed = get_playback_embed();
        break;
    }

    // add page switchingbuttons now
    embed.add_component(
        dpp::component().add_component(
            dpp::component()
                .set_label("Player")
                .set_id("play")
                .set_disabled(page == page_type::playback_control)
        )
        .add_component(
            dpp::component()
                .set_label("Queue")
                .set_id("queue")
                .set_disabled(page == page_type::queue)
        )
        .add_component(
            dpp::component()
                .set_label("History")
                .set_id("history")
                .set_disabled(page == page_type::history)
        )
        .add_component(
            dpp::component()
                .set_label("Add Music")
                .set_id("add")
        )
        .add_component(
            dpp::component()
                .set_label("End")
                .set_id("leave")
                .set_emoji(dpp::unicode_emoji::skull)
                .set_style(dpp::cos_danger)
        )
    );

    return embed;
}

void music_queue::changePageNumber(int inc_value)
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    if (inc_value > 0)
        page_number++;
    else
        page_number--;
}

dpp::message music_queue::get_queue_embed()
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    size_t size = queue.size();
    size_t start = (MAX_EMBED_VALUES * page_number) + 1;
    size_t end = MAX_EMBED_VALUES * (page_number+1);
    // validate page
    if (size < start || start < 0)
    {
        page_number = 0;
        start = 1;
        end = MAX_EMBED_VALUES;
    }
    dpp::embed q_embed = dpp::embed()
        .set_color(dpp::colors::pink_rose)
        .set_title("Queue")
        .set_footer(
            dpp::embed_footer()
                .set_text(std::to_string(size >= 0 ? size-1 : 0) + " songs")
        )
        .set_timestamp(time(0));
    
    if (queue.size() > 0)
    {
        // playing now
        q_embed.add_field("Playing Now:", queue.front().title + "\n[" + queue.front().duration + "]");
        q_embed.set_thumbnail(queue.front().thumbnail);

        q_embed.add_field("", "");
        q_embed.add_field("Up Next:", "");
        int i,j;
        // write inline fields
        for (i = start, j = start + (MAX_EMBED_VALUES / 2); i < queue.size() && j < queue.size() && j <= end; i++, j++)
        {
            q_embed.add_field(std::to_string(i) + ". ", queue.at(i).title + "\n[" + queue.at(i).duration + "]", true);
            q_embed.add_field(std::to_string(j) + ". ", queue.at(j).title + "\n[" + queue.at(j).duration + "]", true);
            q_embed.add_field("", "");
        }
        // write any remaining
        for (; i < queue.size() && i <= end - (MAX_EMBED_VALUES / 2); i++)
            q_embed.add_field(std::to_string(i) + ". ", queue.at(i).title + "\n[" + queue.at(i).duration + "]");
    }
    else
        q_embed.add_field("Empty!", "");

    dpp::message msg = dpp::message(q_embed);

    // return msg w/ components
    return msg.add_component(
        dpp::component().add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::left_arrow)
                .set_id("prev")
                .set_disabled(start < MAX_EMBED_VALUES)
        )
        .add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::right_arrow)
                .set_id("next") // store current page number in id
                .set_disabled(size < end)
        )
        .add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::arrows_clockwise)
                .set_id("shuffle")
        )
    );
}

dpp::message music_queue::get_history_embed()
{
    std::lock_guard<std::mutex> guard(history_mutex);

    size_t size = history.size();
    size_t start = (MAX_EMBED_VALUES * page_number) + 1;
    size_t end = MAX_EMBED_VALUES * (page_number+1);
    // validate page
    if (size < start || start < 0)
    {
        page_number = 0;
        start = 1;
        end = MAX_EMBED_VALUES;
    }
    dpp::embed q_embed = dpp::embed()
        .set_color(dpp::colors::pink_rose)
        .set_title("History")
        .set_timestamp(time(0));
    
    if (history.size() > 0)
    {
        int i,j;
        // write inline fields
        for (i = start, j = start + (MAX_EMBED_VALUES / 2); i < history.size() && j < history.size() && j <= end; i++, j++)
        {
            q_embed.add_field(std::to_string(i) + ". ", history.at(i), true);
            q_embed.add_field(std::to_string(j) + ". ", history.at(j), true);
            q_embed.add_field("", "");
        }
        // write any remaining
        for (; i < history.size() && i <= end - (MAX_EMBED_VALUES / 2); i++)
            q_embed.add_field(std::to_string(i) + ". ", history.at(i));
    }
    else
        q_embed.add_field("Empty!", "");

    dpp::message msg = dpp::message(q_embed);

    // return msg w/ components
    return msg.add_component(
        dpp::component().add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::left_arrow)
                .set_id("prev")
                .set_disabled(start < MAX_EMBED_VALUES)
        )
        .add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::right_arrow)
                .set_id("next") // store current page number in id
                .set_disabled(size < end)
        )
    );
}

dpp::message music_queue::get_playback_embed()
{
    std::lock_guard<std::mutex> guard(queue_mutex);
    
    song song;
    bool noSongs = true;
    if (!queue.empty())
    {
        song = queue.front();
        noSongs = false;
    }
    else
    {
        song.thumbnail = DEFAULT_THUMBNAIL;
        song.title = "No songs playing...";
        song.duration = "0:00";
    }

    dpp::embed embed = dpp::embed()
        .set_color(dpp::colors::pink_rose)
        .set_title("Music Player")
        .set_image(song.thumbnail)
        .add_field(song.title, "[" + song.duration + "]");

    dpp::message msg(embed);

    return msg.add_component(
        dpp::component().add_component(
            // rewind btn jus for show
            dpp::component()
                .set_emoji(dpp::unicode_emoji::rewind)
                .set_id("rewind")
                .set_disabled(true)
        )
        .add_component(
            // play/pause btn
            dpp::component()
                .set_emoji(dpp::unicode_emoji::play_pause)
                .set_id("pause")
                .set_disabled(noSongs)
        )
        .add_component(
            // stop btn
            dpp::component()
                .set_emoji(dpp::unicode_emoji::stop_button)
                .set_id("stop")
                .set_disabled(noSongs)
        )
        .add_component(
            // skip btn
            dpp::component()
                .set_emoji(dpp::unicode_emoji::fast_forward)
                .set_id("skip")
                .set_disabled(noSongs)
        )
        .add_component(
            dpp::component()
                .set_emoji(dpp::unicode_emoji::arrows_clockwise)
                .set_id("shuffle")
        )
    );
}

void music_queue::addHistory(std::string new_entry)
{
    std::lock_guard<std::mutex> guard(history_mutex);

    if (history.size() == MAX_HISTORY_ENTRIES)
        history.pop_front();
    history.push_back(new_entry);
}

void music_queue::shuffle()
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    auto rng_seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(queue.begin()+1, queue.end(), std::default_random_engine(rng_seed));

    if (queue.size() > 1)
    {
        std::thread t([url = queue.at(1).url, this](){ this->preload(url); });
        t.detach();
    }
}

/*This function handles streaming of song to discord. Will release mutex and block if stream is live
to allow queue to still add songs. Else holds mutex until download is finished*/
bool music_queue::handle_download(std::string url)
{
    std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + url + " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet pipe:.pcm";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        return false;
    constexpr size_t buffsize = dpp::send_audio_raw_max_length;
    char buffer[dpp::send_audio_raw_max_length];
    size_t curr_read = 0;
    
    if (!vc)
    {
        std::unique_lock<std::mutex> guard(queue_mutex);
        std::cout << "Waiting for vc\n";
        vc_ready.wait(guard);
        std::cout << "Continuing...\n";
    }

    while (!vc->terminating && !stopLivestream && (curr_read = fread(buffer, sizeof(char), buffsize, pipe.get())) == buffsize)
        vc->send_audio_raw((uint16_t*)buffer, buffsize);

    if (!vc->terminating && !stopLivestream && curr_read > 0)
    {
        int rem = curr_read % 4;
        vc->send_audio_raw((uint16_t*)buffer, curr_read-rem);
    }
    if (!vc->terminating)
        vc->insert_marker("end");

    std::cout << "Finished streaming bytes\n";
    return true;
}

bool music_queue::preload(std::string url)
{
    std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + url + 
    " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet -y " + std::string(NEXT_SONG);
    return system(cmd.c_str()) == 0;
}
