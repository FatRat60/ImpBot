#include "youtube.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdio.h>

// yt-dlp -q --no-warnings -x --audio-format opus -o - -S +size --no-playlist 
// "yt-dlp -f 140 -q --no-warnings -o - --no-playlist " + youtube_song.url + " | ffmpeg -i pipe:.m4a -c:a libopus -ar 48000 -ac 2 -loglevel quiet pipe:.opus";

std::string youtube::YOUTUBE_API_KEY;
std::unordered_map<dpp::snowflake, music_queue*> youtube::queue_map;
std::mutex youtube::queue_map_mutex;

song youtube::get_song_info(std::string& query)
{
    std::string cmd = "yt-dlp -q --no-warnings --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(fulltitle)j,\\\"is_live\\\":%(is_live)j,\\\"thumbnail\\\":%(thumbnail)j}\" " + query;
    song found_song;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe)
    {
        found_song.url = YOUTUBE_VIDEO_URL;
        std::array<char, 1024> buffer;
        std::string data;
        while (fread(buffer.data(), sizeof(char), 1024, pipe.get()) > 0)
            data += buffer.data();

        // clean up data???
        size_t pos = data.rfind('}');

        found_song = create_song("{" + data.substr(1, pos));
    }
    return found_song;
}

song youtube::create_song(std::string data)
{
    song new_song;

    try
    {
        dpp::json json = dpp::json::parse(data);
        new_song.url += json["id"];
        new_song.title = json["title"];
        new_song.duration = json["duration"];
        new_song.thumbnail = json["thumbnail"];
        new_song.type = video;
    }
    catch(const dpp::json::parse_error& e)
    {
        //should def make removing duration: much cleaner
        size_t pos = data.rfind('}');
        dpp::json json = dpp::json::parse("{" + data.substr(15, pos-14));
        new_song.url += json["id"];
        new_song.title = json["title"];
        new_song.duration = "LIVE";
        new_song.thumbnail = json["thumbnail"];
        new_song.type = livestream;
    }
    return new_song;
}

void youtube::handle_video(const dpp::slashcommand_t& event, std::string query)
{
    song new_song = get_song_info(query);

    if (!new_song.url.empty())
    {

        auto voice = event.from->get_voice(event.command.guild_id);
        while (!voice || !voice->voiceclient)
            voice = event.from->get_voice(event.command.guild_id);

        music_queue& queue = *youtube::getQueue(event.command.guild_id, true);
        if (queue.enqueue(voice->voiceclient, new_song))
        {
            event.edit_original_response(dpp::message("Added " + new_song.url));
            return;
        }
    }
    event.edit_original_response(dpp::message("There was an issue queueing that song"));
}

void youtube::handle_playlist(const dpp::slashcommand_t &event, std::string query)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    while (!voice || !voice->voiceclient)
        voice = event.from->get_voice(event.command.guild_id);

    std::string cmd = "yt-dlp -q --no-warnings --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(fulltitle)j,\\\"is_live\\\":%(is_live)j,\\\"thumbnail\\\":%(thumbnail)j}\" " + query;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    size_t num_songs = 0;
    if (pipe)
    {
        music_queue& queue = *getQueue(event.command.guild_id, true);
        std::array<char, 512> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            song new_song = create_song(buffer.data());
            num_songs += !new_song.url.empty();
            queue.enqueue(voice->voiceclient, new_song);
        }
    }

    event.edit_original_response(dpp::message(std::to_string(num_songs) + " songs added from " + query));
}

void youtube::play(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    // join voice channel first
    dpp::guild *g = dpp::find_guild(event.command.guild_id);
    auto current_vc = event.from->get_voice(event.command.guild_id);
    bool join_vc = true;
    bool doMusic = true;

    if (current_vc)
    {
        auto users_vc = g->voice_members.find(event.command.get_issuing_user().id);
        if (users_vc != g->voice_members.end() && current_vc->channel_id != users_vc->second.channel_id)
        {
            event.from->disconnect_voice(event.command.guild_id);
        }
        else 
            join_vc = false;
    }

    if (join_vc)
    {
        if (!g->connect_member_voice(event.command.get_issuing_user().id))
        {
            event.reply("You are not in a voice channel, idiot...");
            doMusic = false;
        }
    }

    // query youtube if joined channel AND API key is init'd
    if (doMusic)
    {
        std::string search_term = std::get<std::string>(event.get_parameter("link"));
        if (!YOUTUBE_API_KEY.empty()) // Youtube API key exists
        {
            event.reply("Searching for " + search_term,
            [event, search_term](const dpp::confirmation_callback_t& callback){
                std::string link = search_term;
                // user sent a link
                if (search_term.substr(0, 8) == "https://")
                {
                    size_t slash = search_term.find('/', 8);
                    std::string platform = link.substr(8, slash - 8);
                    // youtube
                    if (platform == "www.youtube.com" || platform == "youtube.com" || platform == "youtu.be")
                    {
                        size_t mark = search_term.find('?', slash);
                        size_t aaron = search_term.find("&", mark);
                        link = search_term.substr(0, aaron);
                        std::string type = link.substr(slash+1, mark - (slash+1)); 
                        
                        if (type == "playlist")
                        {
                            handle_playlist(event, link);
                        }
                        else // video or livestream
                        {
                            handle_video(event, link);
                        }
                    }
                    // youtube music
                    else if (platform == "music.youtube.com")
                    {
                        size_t mark = search_term.find('?', slash);
                        std::string type = search_term.substr(slash+1, mark-(slash+1));
                        size_t equals = search_term.find('=', mark);
                        size_t aaron = search_term.find('&', mark);
                        std::string id = search_term.substr(equals+1, aaron - (equals+1));
                        if (type == "playlist")
                        {
                            handle_playlist(event, std::string(YOUTUBE_LIST_URL) + id);
                        }
                        else
                        {
                            handle_video(event, std::string(YOUTUBE_VIDEO_URL) + id);
                        }
                    }
                    // spotify
                    else if (platform == "open.spotify.com")
                    {
                        event.edit_original_response(dpp::message("Spotify support coming soon"));
                    }
                    // soundcloud
                    else if (platform == "soundcloud.com")
                    {
                        event.edit_original_response(dpp::message("Soundcloud support coming soon"));
                    }
                    else
                        event.edit_original_response(dpp::message("The platform, " + platform + ", is not currently supported"));
                }
                // user sent a query. Search youtube
                else
                {
                    handle_video(event, "\"ytsearch:" + search_term + ":1\"");
                }
            });
        }
        else // API key not found.
        {
            event.reply("Youtube search not available. Tell kyle's dumbass to regenerate the API key");
            return;
        }
    }
}

void youtube::pause(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            bool paused = voice_client->is_paused();
            voice_client->pause_audio(!paused);
            std::string append = paused ? "resumed" : "paused";
            event.reply("Song " + append);
        }
        else
        {
            event.reply("No song playing");
        }
    }
    else
    {
        event.reply("Im not in a voice channel IDIOT");
    }
}

void youtube::stop(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            music_queue& queue = *getQueue(event.command.guild_id);
            queue.clear_queue();
            event.reply("Clearing queue...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            voice_client->stop_audio();

            event.edit_original_response(dpp::message("Songs stopped and removed from queue my lord"));
        }
        else
            event.reply("No songs playing");
    }
    else
        event.reply("Jesus man im not in a channel dummy");
}

void youtube::skip(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            event.reply("Skipped");
            std::thread t([voice_client]() {
                music_queue& queue = *youtube::getQueue(voice_client->server_id);
                queue.skip(voice_client);
            });
            t.detach();
        }
        else
        {
            event.reply("No songs playing");
        }
    }
    else
        event.reply("Tip: join a voice channel before typing /skip");
}

void youtube::queue(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn) // bot is connected
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue && !queue->empty()) // there is actually music playing
        {
            // send embed
            event.reply(queue->get_queue_embed());
        }
        else
            event.reply("No songs in queue");
    }
    else
        event.reply("Not in a channel pookie. Such a silly billy");
}

void youtube::remove(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn)
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue)
        {
            std::string numbers = std::get<std::string>(event.get_parameter("number"));
            if (!numbers.empty())
            {
                size_t num = std::stol(numbers.substr(0, numbers.size()));
                event.reply("Removing track number " + numbers + "...");
                if (queue->remove_from_queue(num)) // number to remove is within the range of songs queued
                {
                    event.edit_original_response(dpp::message("Track " + numbers + " was removed from queue"));
                }
                else
                    event.edit_original_response(dpp::message("Could not remove track " + numbers));
            }
        }
        else
            event.reply("No songs ");
    }
    else
        event.reply("Not in voice");
}

music_queue* youtube::getQueue(const dpp::snowflake guild_id, bool create/* = false */)
{
    std::lock_guard<std::mutex> guard(queue_map_mutex);

    auto res = queue_map.find(guild_id);
    music_queue* found_queue = nullptr;
    if (res == queue_map.end())
    {
        if (create)
        {
            found_queue = new music_queue();
            queue_map.insert({guild_id, found_queue});
        }
    }
    else
        found_queue = res->second;

    return found_queue;
}

void youtube::handle_marker(const dpp::voice_track_marker_t &marker)
{
    // Checks if marker is to be skipped. If so will invoke skip_to_next_marker()
    // Parse marker for number
    if (marker.track_meta == "end") // found space
    {
        std::thread t([marker]() {
            if (!marker.voice_client->terminating)
            {
                music_queue* queue = youtube::getQueue(marker.voice_client->server_id);
                if (queue)
                    queue->go_next(marker.voice_client);
            }
        });
        t.detach();
    }
}

void youtube::handle_voice_leave(const dpp::slashcommand_t& event)
{
    std::unique_ptr<music_queue> queue(getQueue(event.command.guild_id));
    queue_map.erase(event.command.guild_id);
    queue->clear_queue();
}

void youtube::handle_button_press(const dpp::button_click_t &event)
{
    std::thread t([event](){
        size_t index = event.custom_id.find('_');
        // there is a var attached to id
        std::string id = event.custom_id.substr(0, index);
        // button press to update queue
        if (id == "queue")
        {
            // either next or prev
            if (index != std::string::npos)
            {
                size_t page;
                try
                {
                    page = std::stoul(event.custom_id.substr(index+1));
                }
                catch(const std::exception& e)
                {
                    page = 0;
                }
                music_queue* queue = getQueue(event.command.guild_id);
                dpp::message msg;
                if (queue)
                    msg = queue->get_queue_embed(page);
                else
                    msg = dpp::message("Why would you click this knowing there's no music playing? Dumbass...");
                event.reply(dpp::ir_update_message, msg);
            }
        }
    });
    t.detach();
}
