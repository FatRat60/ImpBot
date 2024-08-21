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
    std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\\n\" " + query;
    song found_song;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe)
    {
        char buffer[1024];
        if (fgets(buffer, 1024, pipe.get()) != nullptr)
        {
            std::string data = buffer;
            // remove newline
            if (data.rfind('\n') != std::string::npos)
                data.pop_back();
            found_song = create_song(data);
        }
    }
    return found_song;
}

song youtube::create_song(std::string data)
{
    song new_song;
    
    try
    {
        dpp::json json = dpp::json::parse(data);
        new_song.url = YOUTUBE_VIDEO_URL;
        new_song.title = json["title"];
        // livestream
        if (json["duration"] == "")
        {
            new_song.type = livestream;
            new_song.duration = "LIVE";
        }
        // normal video
        else
        {
            new_song.type = video;
            new_song.duration = json["duration"];
        }
        new_song.thumbnail = json["thumbnail"];
        new_song.url += json["id"];
    }
    catch(const dpp::json::parse_error& e)
    {
        // if errors for whatever reason jus dont queue song. But it shouldnt now
        new_song.title = "";
    }
    return new_song;
}

void youtube::handle_video_youtube(const dpp::slashcommand_t& event, std::string videoId, bool doReply)
{
    if (!YOUTUBE_API_KEY.empty())
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") + "&id=" + videoId + "&key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, videoId, doReply](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    // parse body
                    dpp::json json = dpp::json::parse(reply.body);
                    music_queue& queue = *youtube::getQueue(event.command.guild_id, true);

                    // get voice
                    auto voice = event.from->get_voice(event.command.guild_id);
                    while (!voice || !voice->voiceclient)
                        voice = event.from->get_voice(event.command.guild_id);

                    // create song struct and enqueue it
                    song new_song;
                    new_song.url = YOUTUBE_VIDEO_URL;
                    new_song.title = json["items"][0]["snippet"]["title"];
                    new_song.thumbnail = json["items"][0]["snippet"]["thumbnails"]["default"]["url"];
                    new_song.duration = convertDuration(json["items"][0]["contentDetails"]["duration"]);
                    //new_song.duration = json["items"][0]["contentDetails"]["duration"];
                    new_song.type = (song_type)(json["items"][0]["snippet"]["liveBroadcastContent"] != "none");
                    new_song.url += videoId;

                    bool res = queue.enqueue(voice->voiceclient, new_song);
                    if (doReply)
                    {
                        if (res)
                            event.edit_original_response(dpp::message("Added " + new_song.url));
                        else
                            event.edit_original_response(dpp::message("There was an issue queueing that song"));
                    }
                }
                // rate-limit
                else if (reply.status == 429)
                {
                    YOUTUBE_API_KEY = "";
                    // handle_video_dlp
                    youtube::handle_video_dlp(event, videoId, doReply);
                }
                else if (doReply)
                    event.edit_original_response(dpp::message("There was an issue queuing the video with id: " + videoId));
            }
        );
    else
        handle_video_dlp(event, videoId, doReply);
}

void youtube::handle_playlist_youtube(const dpp::slashcommand_t &event, std::string playlistId)
{
    if (!YOUTUBE_API_KEY.empty())
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&playlistId=" + playlistId + "&maxResults=50&key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, playlistId](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    // parse body
                    dpp::json json = dpp::json::parse(reply.body);
                    youtube::handle_playlist_item(event, json);
                }
                else if (reply.status == 429)
                {
                    YOUTUBE_API_KEY = "";
                    youtube::handle_playlist_dlp(event, playlistId);
                }
                else
                    event.edit_original_response(dpp::message("There was an issue queueing playlist with id: " + playlistId));
            }
        );
    else
        handle_playlist_dlp(event, playlistId);
}

void youtube::handle_playlist_item(const dpp::slashcommand_t& event, dpp::json& playlistItem, size_t songs)
{
    for (dpp::json item : playlistItem["items"])
    {
        songs++;
        handle_video_youtube(event, item["snippet"]["resourceId"]["videoId"], false);
    }

    std::string playlistId = playlistItem["items"][0]["snippet"]["playlistId"];
    if (playlistItem.contains("nextPageToken") && !YOUTUBE_API_KEY.empty())
    {
        std::string nextPageToken = playlistItem["nextPageToken"];
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&playlistId=" + playlistId + "&maxResults=50&nextPageToken=" + nextPageToken + "key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, songs, playlistId](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    dpp::json json = dpp::json::parse(reply.body);
                    youtube::handle_playlist_item(event, json, songs);
                }
                else
                {
                    YOUTUBE_API_KEY = "";
                    event.edit_original_response(dpp::message("Added " + std::to_string(songs) + " from " + std::string(YOUTUBE_LIST_URL) + playlistId));
                }

            }
        );
    }
    else
        event.edit_original_response(dpp::message("Added " + std::to_string(songs) + " from " + std::string(YOUTUBE_LIST_URL) + playlistId));
}

void youtube::handle_video_dlp(const dpp::slashcommand_t &event, std::string videoId, bool doReply)
{
    std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\" " + std::string(YOUTUBE_VIDEO_URL) + videoId;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    char buffer[256];
    auto voice = event.from->get_voice(event.command.guild_id);
    while (!voice || !voice->voiceclient)
        voice = event.from->get_voice(event.command.guild_id);

    if (pipe)
    {
        fgets(buffer, 256, pipe.get());
        std::string data = buffer;
        data.pop_back();
        song new_song = create_song(data);
        if (!new_song.title.empty())
        {
            music_queue& queue = *youtube::getQueue(event.command.guild_id, true);
            bool res = queue.enqueue(voice->voiceclient, new_song);
            if (doReply)
            {
                if (res)
                        event.edit_original_response(dpp::message("Added " + new_song.url));
                else
                        event.edit_original_response(dpp::message("There was an issue queueing that song"));
            }
        }
        else if (doReply)
            event.edit_original_response(dpp::message("Songs is hidden or unavailable"));
    }
    else if (doReply)
        event.edit_original_response(dpp::message("Broken Pipe"));
}

void youtube::handle_playlist_dlp(const dpp::slashcommand_t &event, std::string playlistId)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    while (!voice || !voice->voiceclient)
        voice = event.from->get_voice(event.command.guild_id);

    std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --flat-playlist --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\" " + std::string(YOUTUBE_LIST_URL) + playlistId;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    size_t num_songs = 0;
    if (pipe)
    {
        music_queue& queue = *getQueue(event.command.guild_id, true);
        std::array<char, 512> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            song new_song = create_song(buffer.data());
            if (new_song.title != "")
            {
                num_songs++;
                queue.enqueue(voice->voiceclient, new_song);
            }
        }
    }

    event.edit_original_response(dpp::message(std::to_string(num_songs) + " songs added from " + playlistId));
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
        event.reply("Searching for " + search_term,
        [event, search_term](const dpp::confirmation_callback_t& callback){
            std::string link = search_term;
            // user sent a link
            if (search_term.substr(0, 8) == "https://")
            {
                size_t slash = search_term.find('/', 8);
                std::string platform = link.substr(8, slash - 8);
                // youtube
                if (platform == "www.youtube.com" || platform == "youtube.com" || platform == "youtu.be" || platform == "music.youtube.com")
                {
                    size_t mark = search_term.find('?', slash);
                    std::string type = search_term.substr(slash+1, (mark-slash)-1);
                    size_t equals = search_term.find('=', mark);
                    size_t aaron = search_term.find('&', equals);
                    std::string id = search_term.substr(equals+1, (aaron-equals) - 1);
                    
                    // /playlist?list=<id>
                    if (type == "playlist")
                    {
                        handle_playlist_youtube(event, id);
                    }
                    // ?v=<id>
                    else if (type == "watch")
                    {
                        handle_video_youtube(event, id); 
                    }
                    // either /live/<id>? or /<id>?
                    else
                    {
                        id = type;
                        size_t slash2 = type.find('/');
                        if (slash2 == std::string::npos)
                            id = type.substr(slash2+1);
                        handle_video_youtube(event, id);
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
                std::string cmd = "yt-dlp -q --no-warnings --flat-playlist --no-playlist --print \"id\" \"ytsearch1:" + search_term + "\"";
                char buffer[50];
                std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
                if (pipe)
                { 
                    fgets(buffer, 50, pipe.get());
                    std::string id = buffer;
                    id.pop_back();
                    handle_video_youtube(event, id);
                }
                else
                    event.edit_original_response(dpp::message("Broken Pipe"));
            }
        });
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
            // parse start and end
            std::string first, second;
            size_t colon = numbers.find(':');
            if (colon != std::string::npos)
            {
                first = numbers.substr(0, colon);
                second = numbers.substr(colon);
                second.replace(0, 1, "");
            }
            // sanitize them
            if (!first.empty() && isdigit(first[0]))
            {
                size_t start = std::stoul(first);
                size_t end = std::string::npos;
                if (!second.empty())
                {
                    if (isdigit(second[0]))
                        end = std::stoul(second);
                    else
                        end = start+1;
                }
                if (queue->remove_from_queue(start, end))
                {
                    event.reply("removed track(s) from queue");
                }
                else
                    event.reply("could not remove track(s)");
            }
            else
                event.reply("Invalid or non-existant first parameter");
        }
        else
            event.reply("No songs");
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

std::string youtube::convertDuration(std::string old_duration)
{
    std::string new_duration;
    if (old_duration == "POD")
        new_duration = "LIVE";
    else if (!old_duration.empty() && old_duration[0] == 'P')
    {   // convert from ISO-8601
        std::string old_duration_front = old_duration.substr(0, old_duration.find('T'));
        std::string letters = "PYMWD";
        std::array<size_t, 4> conversions = {8760, 730, 168, 24};
        size_t start = 0;
        size_t curr = 1;
        size_t hours = 0;
        while (curr < 4)
        {
            size_t pos = old_duration_front.find(letters[curr], start);
            if (pos != std::string::npos)
            {
                std::string num = old_duration_front.substr(start, (pos - start) - 1);
                hours += (std::stoul(num) * conversions[curr]);
                start = pos; 
            }
            curr++;
        }
        // parse hours
        start = 0;
        old_duration = old_duration.substr(old_duration.find('T'));
        curr = old_duration.find('H', start);
        if (curr != std::string::npos)
        {
            std::cout << "stoul: " + old_duration.substr(start+1, (curr-start) - 1) << "\n";
            hours += std::stoul(old_duration.substr(start+1, (curr-start) - 1));
            start = curr;
        }
        if (hours > 0)
            new_duration += std::to_string(hours) + ":";
        
        // parse minutes
        curr = old_duration.find('M', start);
        if (curr != std::string::npos)
        {
            new_duration += old_duration.substr(start+1, (curr-start) - 1) + ":";
            start = curr;
        }
        else
            new_duration += "0:";

        // parse seconds
        curr = old_duration.find('S', start);
        std::string sec = "00";
        if (curr != std::string::npos)
            sec = old_duration.substr(start+1, (curr-start)-1);
        if (sec.length() < 2)
            sec = "0" + sec;
        new_duration += sec;
    }
    return new_duration;
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
        size_t under = event.custom_id.find('_');
        std::string cmd = event.custom_id.substr(0, under);
        // button press to update queue
        if (cmd == "queue")
        {
            dpp::message msg;
            music_queue* queue = getQueue(event.command.guild_id);
            if (queue)
            {
                // refresh
                if (under == std::string::npos)
                    msg = queue->get_queue_embed();
                // prev/next
                else
                {
                    size_t page = queue->getPage();
                    if (event.custom_id.substr(under+1) == "next")
                        page++;
                    else
                        page--;
                    msg = queue->new_page(page);
                }
            }
            else
                msg = dpp::message("Why would you click this knowing there's no music playing? Dumbass...");

            event.reply(dpp::ir_update_message, msg);     
        }
        else if (cmd == "shuffle")
        {
            music_queue* queue = youtube::getQueue(event.command.guild_id);
            if (queue)
            {
                queue->shuffle();
                event.reply(dpp::ir_update_message, queue->get_queue_embed());
            }   
            else
                event.reply(dpp::ir_update_message, dpp::message("No queue"));
        }
    });
    t.detach();
}

void youtube::shuffle(const dpp::slashcommand_t &event)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    if (voice)
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue)
        {
            queue->shuffle();
            event.reply("Shuffled!");
        }
        else
            event.reply("Nothing to shuffle dumbass");
    }   
    else
        event.reply("Not in voice");
}
