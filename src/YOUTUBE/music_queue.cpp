#include "music_queue.h"
#include <dpp/unicode_emoji.h>

/*If first song: Download and send song to discord, then add to queue
If second song, will download locally to resources/next.pcm, then add to queue
Else, will just add to queue. No download is performed yet.
Returns true if download is succesful AND song is added to queue, else false*/
bool music_queue::enqueue(dpp::discord_voice_client* vc, song& song_to_add)
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
        std::thread t([vc, this](){ this->handle_download(vc); });
        t.detach();
    }
    else if (queue.size() == 2)
    {
        // preload
        std::thread t([this](){ this->preload(); });
        t.detach();
    }
    return true;
}

/*Will be called when streamed song is finished. Pops top of queue
then begins streaming next song which should be located on disk at resources/next.pcm
After streaming to discord, download next song if exists to resources/next.pcm
If no track is next, return false, else true*/
bool music_queue::go_next(dpp::discord_voice_client* vc)
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    if (!queue.empty())
    {
        queue.pop_front();

        if (!queue.empty())
        {
            // send next song
            song next = queue.front();
            if (next.type == livestream)
            {
                stopLivestream = false;
                std::thread t([vc, this](){ this->handle_download(vc); });
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
            preload();
            return true;
        }
    }
    return false;
}

void music_queue::skip(dpp::discord_voice_client* vc)
{
    if (!queue.empty() && queue.front().type == livestream)
    {
        // need to signal flag to end livestream. Afterwhich a marker will get added and trigger
        std::lock_guard<std::mutex> guard(queue_mutex);
        stopLivestream = true;
        vc->skip_to_next_marker();
    }
    else // jus skipping a song
    {
        vc->skip_to_next_marker();
        go_next(vc);
    }
}

void music_queue::clear_queue()
{
    std::lock_guard<std::mutex> guard(queue_mutex);
    stopLivestream = true; // terminate livestream
    queue.clear();
}

bool music_queue::remove_from_queue(size_t ind)
{
    std::lock_guard<std::mutex> guard(queue_mutex);
    if (ind < queue.size())
    {
        queue.erase(queue.begin() + ind);

        if (ind == 1)
            preload();
        return true;
    }
    return false;
}

dpp::message music_queue::get_queue_embed(size_t page/* = 0 */)
{
    std::lock_guard<std::mutex> guard(queue_mutex);

    size_t size = queue.size();
    size_t start = (MAX_EMBED_VALUES * page) + 1;
    size_t end = MAX_EMBED_VALUES * (page+1);
    // validate page
    if (size < start)
    {
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

        for (int i = start; i < queue.size() && i <= end; i++)
        {
            q_embed.add_field(std::to_string(i) + ". ", queue.at(i).title + "\n[" + queue.at(i).duration + "]");
        }
    }
    else
        q_embed.add_field("Empty!", "");

    dpp::message msg = dpp::message(q_embed);
    // whether to add next page button
    if (size > end)
        msg.add_component(
            dpp::component().add_component(
                dpp::component()
                    .set_type(dpp::cot_button)
                    .set_emoji(dpp::unicode_emoji::right_arrow)
                    .set_id("queue_" + std::to_string(page+1)) // store current page number in id
            )
        );
    // whether to add prev page button
    if (start > MAX_EMBED_VALUES)
        msg.add_component(
            dpp::component().add_component(
                dpp::component()
                    .set_emoji(dpp::unicode_emoji::left_arrow)
                    .set_id("queue_" + std::to_string(page-1))
            )
        );
    // return msg w/ components
    return msg;
}

/*This function handles streaming of song to discord. Will release mutex and block if stream is live
to allow queue to still add songs. Else holds mutex until download is finished*/
bool music_queue::handle_download(dpp::discord_voice_client* vc)
{
    song song = queue.front();
    std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + song.url + " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet pipe:.pcm";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        return false;
    constexpr size_t buffsize = dpp::send_audio_raw_max_length;
    char buffer[dpp::send_audio_raw_max_length];
    size_t curr_read = 0;

    while (!vc->terminating && !stopLivestream && (curr_read = fread(buffer, sizeof(char), buffsize, pipe.get())) == buffsize)
        vc->send_audio_raw((uint16_t*)buffer, buffsize);

    if (!vc->terminating && !stopLivestream && curr_read > 0)
    {
        int rem = curr_read % 4;
        vc->send_audio_raw((uint16_t*)buffer, curr_read-rem);
    }
    if (!vc->terminating)
        vc->insert_marker("end");

    return true;
}

bool music_queue::preload()
{
    if (queue.size() > 1)
    {
        song song = queue.at(1);
        if (song.type != livestream)
        {
            std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + song.url + 
            " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet -y " + std::string(NEXT_SONG);
            return system(cmd.c_str()) == 0;
        }
        return true;
    }
    else
        return false;
}
