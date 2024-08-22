#include "youtube.h"
#include "spotify.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdio.h>

// yt-dlp -q --no-warnings -x --audio-format opus -o - -S +size --no-playlist 
// "yt-dlp -f 140 -q --no-warnings -o - --no-playlist " + youtube_song.url + " | ffmpeg -i pipe:.m4a -c:a libopus -ar 48000 -ac 2 -loglevel quiet pipe:.opus";

std::string youtube::YOUTUBE_API_KEY;

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

void youtube::parseURL(const dpp::slashcommand_t& event, std::string link, music_queue *queue)
{
    link.erase(0, 1);
    size_t mark = link.find('?');
    std::string type = link.substr(0, mark);
    size_t equals = link.find('=', mark);
    size_t aaron = link.find('&', equals);
    std::string id = link.substr(equals+1, (aaron-equals) - 1);
    
    // /playlist?list=<id>
    if (type == "playlist")
    {
        youtube::handle_playlist(event, id, queue);
    }
    // ?v=<id>
    else if (type == "watch")
    {
        youtube::handle_video(event, id, queue); 
    }
    // either /live/<id>? or /<id>?
    else
    {
        id = type;
        size_t slash2 = type.find('/');
        if (slash2 == std::string::npos)
            id = type.substr(slash2+1);
        youtube::handle_video(event, id, queue);
    }
}

void youtube::ytsearch(const dpp::slashcommand_t &event, std::string query, music_queue *queue, bool doReply)
{
    std::string cmd = "yt-dlp -q --no-warnings --flat-playlist --no-playlist --print \"id\" \"ytsearch1:" + query + "\"";
    char buffer[50];
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe)
    { 
        fgets(buffer, 50, pipe.get());
        std::string id = buffer;
        id.pop_back();
        if (!id.empty())
            youtube::handle_video(event, id, queue, doReply);
        else if (doReply)
            event.edit_original_response(dpp::message("No results returned for: " + query));
    }
    else if (doReply)
        event.edit_original_response(dpp::message("Broken Pipe"));
}

void youtube::handle_video(const dpp::slashcommand_t &event, std::string videoId, music_queue *queue, bool doReply)
{
    if (!YOUTUBE_API_KEY.empty())
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") + "&id=" + videoId + "&key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, videoId, doReply, queue](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    // parse body
                    dpp::json json = dpp::json::parse(reply.body);

                    if (json["items"][0]["snippet"]["title"] == "Deleted video")
                    {
                        event.edit_original_response(dpp::message("Cannot queue deleted Video"));
                        return;
                    }

                    // get voice
                    auto voice = event.from->get_voice(event.command.guild_id);
                    while (!voice || !voice->voiceclient)
                        voice = event.from->get_voice(event.command.guild_id);

                    // create song struct and enqueue it
                    try
                    {
                        song new_song;
                        new_song.url = YOUTUBE_VIDEO_URL;
                        new_song.title = json["items"][0]["snippet"]["title"];
                        new_song.thumbnail = json["items"][0]["snippet"]["thumbnails"]["default"]["url"];
                        new_song.duration = convertDuration(json["items"][0]["contentDetails"]["duration"]);
                        new_song.type = (song_type)(json["items"][0]["snippet"]["liveBroadcastContent"] != "none");
                        new_song.url += videoId;
                        bool res = queue->enqueue(voice->voiceclient, new_song);
                        if (doReply)
                        {
                            if (res)
                                event.edit_original_response(dpp::message("Added " + new_song.url));
                            else
                                event.edit_original_response(dpp::message("There was an issue queueing that song"));
                    }
                    }
                    catch(const std::exception& e)
                    {
                        std::cout << json.dump();
                    }
                }
                // rate-limit
                else if (reply.status == 429)
                {
                    YOUTUBE_API_KEY = "";
                    // handle_video_dlp
                    youtube::handle_video_dlp(event, videoId, queue, doReply);
                }
                else if (doReply)
                    event.edit_original_response(dpp::message("There was an issue queuing the video with id: " + videoId));
            }
        );
    else
        handle_video_dlp(event, videoId, queue, doReply);
}

void youtube::handle_playlist(const dpp::slashcommand_t &event, std::string playlistId, music_queue* queue)
{
    if (!YOUTUBE_API_KEY.empty())
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&playlistId=" + playlistId + "&maxResults=50&key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, playlistId, queue](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    // parse body
                    dpp::json json = dpp::json::parse(reply.body);
                    youtube::handle_playlist_item(event, json, queue);
                }
                else if (reply.status == 429)
                {
                    YOUTUBE_API_KEY = "";
                    youtube::handle_playlist_dlp(event, playlistId, queue);
                }
                else
                    event.edit_original_response(dpp::message("There was an issue queueing playlist with id: " + playlistId));
            }
        );
    else
        handle_playlist_dlp(event, playlistId, queue);
}

void youtube::handle_playlist_item(const dpp::slashcommand_t& event, dpp::json& playlistItem, music_queue* queue, size_t songs)
{
    for (dpp::json item : playlistItem["items"])
    {
        if (item["snippet"]["title"] != "Deleted video")
        {
            songs++;
            handle_video(event, item["snippet"]["resourceId"]["videoId"], queue, false);
        }
    }

    std::string playlistId = playlistItem["items"][0]["snippet"]["playlistId"];
    if (playlistItem.contains("nextPageToken") && !YOUTUBE_API_KEY.empty())
    {
        std::string nextPageToken = playlistItem["nextPageToken"];
        event.from->creator->request(
            std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&playlistId=" + playlistId + "&maxResults=50&pageToken=" + nextPageToken + "&key=" + YOUTUBE_API_KEY,
            dpp::m_get, [event, songs, playlistId, queue](const dpp::http_request_completion_t& reply){
                if (reply.status == 200)
                {
                    dpp::json json = dpp::json::parse(reply.body);
                    youtube::handle_playlist_item(event, json, queue, songs);
                }
                else if (reply.status == 429)
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

void youtube::handle_video_dlp(const dpp::slashcommand_t &event, std::string videoId, music_queue* queue, bool doReply)
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
            bool res = queue->enqueue(voice->voiceclient, new_song);
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

void youtube::handle_playlist_dlp(const dpp::slashcommand_t &event, std::string playlistId, music_queue* queue)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    while (!voice || !voice->voiceclient)
        voice = event.from->get_voice(event.command.guild_id);

    std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --flat-playlist --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\" " + std::string(YOUTUBE_LIST_URL) + playlistId;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    size_t num_songs = 0;
    if (pipe)
    {
        std::array<char, 512> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            song new_song = create_song(buffer.data());
            if (new_song.title != "")
            {
                num_songs++;
                queue->enqueue(voice->voiceclient, new_song);
            }
        }
    }

    event.edit_original_response(dpp::message(std::to_string(num_songs) + " songs added from " + playlistId));
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
    else
        new_duration = old_duration;
    return new_duration;
}
