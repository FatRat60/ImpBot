#include "youtube.h"
#include "spotify.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdio.h>

// yt-dlp -q --no-warnings -x --audio-format opus -o - -S +size --no-playlist 
// "yt-dlp -f 140 -q --no-warnings -o - --no-playlist " + youtube_song.url + " | ffmpeg -i pipe:.m4a -c:a libopus -ar 48000 -ac 2 -loglevel quiet pipe:.opus";

std::string youtube::YOUTUBE_API_KEY;
std::mutex youtube::token_mutex;

void youtube::makeRequest(std::pair<dpp::cluster&, dpp::snowflake> event, std::string endpoint, std::string history_entry, size_t songs)
{
    std::lock_guard<std::mutex> guard(token_mutex);
    // call api if key exists
    if (!YOUTUBE_API_KEY.empty())
        event.first.request(
            endpoint + YOUTUBE_API_KEY, dpp::m_get,
            [event, songs, history_entry](const dpp::http_request_completion_t& reply){
                handleReply(event, reply, history_entry, songs);
            }
        );
    else // no api key, rate-limited. use yt-dlp
    {
        // spawn thread so mutex does not stay locked
        std::thread t([event, endpoint, history_entry](){
            size_t slash = endpoint.rfind('/');
            size_t mark = endpoint.find('?', slash);
            std::string type = endpoint.substr(slash, mark-slash);
            size_t aaron = endpoint.rfind('&');
            size_t equals = endpoint.rfind('=', aaron);
            dpp::json item;
            item["id"] = endpoint.substr(equals+1, (aaron-equals)-1);
            if (type == "/playlistItems" || type == "/playlists")
            {
                handlePlaylist(event, item, history_entry);
            }
            else
            {
                handleVideo(event, item, history_entry);
            }
        });
        t.detach();
    }
}

void youtube::handleReply(std::pair<dpp::cluster&, dpp::snowflake> event, const dpp::http_request_completion_t& reply, std::string history_entry, size_t songs)
{
    // we are bing. parse body and either call handleVideo or handlePlaylist
    if (reply.status == 200)
    {
        dpp::json json = dpp::json::parse(reply.body);

        std::string response_type = json["kind"].get<std::string>();
        if (response_type == "youtube#videoListResponse")
        {
            // Some1 provided a link to a priv/deleted video
            if (!json["items"].empty())
                handleVideo(event, json["items"][0], history_entry);
        }
        else if (response_type == "youtube#playlistListResponse")
            makeRequest(event, 
                std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&maxResults=50&playlistId=" + json["items"][0]["id"].get<std::string>()
                + "&key=", history_entry + " queued from " + json["items"][0]["snippet"]["title"].get<std::string>()
            );
        else
            handlePlaylist(event, json, history_entry, songs);
    }
    // ruh roh we got ratelimited. Set api key to "" and set timer for midnight which will reinstate api key
    else if (reply.status == 429)
    {
        std::lock_guard<std::mutex> guard(token_mutex);
        YOUTUBE_API_KEY = "";
        // TODO: set timer to reinstate key
    }
}

void youtube::handleVideo(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& video, std::string history_entry)
{
    // use yt-dlp to get json
    if (!video.contains("snippet"))
    {
        std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\" ";
        cmd += std::string(YOUTUBE_VIDEO_URL) + video["id"].get<std::string>();
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        char buffer[256];

        // ensure pipe works
        if (!pipe)
        {
            return;
        }  

        fgets(buffer, 256, pipe.get());
        std::string data = buffer;
        data.pop_back();
        // 
        if (data.empty())
        {
            return;
        }

        // genereate json obj
        video = dpp::json::parse(data);
    }

    song new_song = createSong(video);

    // queue the song
    if (!new_song.url.empty())
    {
        music_queue* queue = music_queue::getQueue(event.second);
        if (queue)
        {
            // always update first queue otherwise dont
            bool doUpdate = queue->enqueue(new_song);
            if (!history_entry.empty())
            {
                queue->addHistory(history_entry + " queued " + new_song.title + ". (YouTube)");
            }
            if (doUpdate)
                music_queue::updateMessage(event);
        }
    }
}

void youtube::handlePlaylist(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, std::string history_entry, size_t songs)
{
    // json from youtube api
    if (playlist.contains("items"))
    {
        std::cout << "Playlist\n";
        // iterate through each video
        for (dpp::json video : playlist["items"])
        {
            // dont waste out api tokens :) no thumbnails means priv or deleted usually
            if (!video["snippet"]["thumbnails"].empty())
            {
                songs++;
                makeRequest(event, 
                std::string(YOUTUBE_ENDPOINT) + "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") 
                + "&id=" + video["snippet"]["resourceId"]["videoId"].get<std::string>() + "&key=");
            }
        }
        std::cout << "done with playlist\n";

        // next page of songs to add
        if (playlist.contains("nextPageToken"))
        {
            makeRequest(event, 
                std::string(YOUTUBE_ENDPOINT) + "/playlistItems?part=snippet&playlistId=" 
                + playlist["items"][0]["snippet"]["playlistId"].get<std::string>()
                + "&maxResults=50&pageToken=" + playlist["nextPageToken"].get<std::string>() + "&key=",
                history_entry, songs);
        }
        else if (!history_entry.empty())
        {
            music_queue* queue = music_queue::getQueue(event.second);
            if (queue)
                queue->addHistory(history_entry + " " + std::to_string(songs) + " songs (YouTube)");
        }
    }
    else
    {
        std::string cmd = "yt-dlp -q --no-warnings --output-na-placeholder \"\\\"\\\"\" --flat-playlist --print \"{\\\"duration\\\":%(duration_string)j,\\\"id\\\":%(id)j,\\\"title\\\":%(title)j, \\\"thumbnail\\\":%(thumbnail)j}\" "
            + std::string(YOUTUBE_LIST_URL) + playlist["id"].get<std::string>();
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        size_t num_songs = 0;
        if (pipe)
        {
            std::array<char, 512> buffer;
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            {
                dpp::json json = dpp::json::parse(buffer.data());
                song new_song = createSong(json);
                if (new_song.title != "")
                {
                    music_queue* queue = music_queue::getQueue(event.second);
                    if (queue)
                    {
                        num_songs++;
                        queue->enqueue(new_song);
                    }
                    else
                        break;
                }
            }
            if (!history_entry.empty())
            {
            music_queue* queue = music_queue::getQueue(event.second);
            if (queue)
                queue->addHistory(history_entry + " " + std::to_string(songs) + " songs (yt-dlp)");
            }
        }
    }
}

song youtube::createSong(dpp::json& video)
{
    song new_song;

    new_song.url = YOUTUBE_VIDEO_URL;

    if (video.contains("snippet"))
    {
        if (video["snippet"]["title"].get<std::string>() != "Deleted video")
        {
            new_song.url = YOUTUBE_VIDEO_URL;
            new_song.title = video["snippet"]["title"];
            new_song.thumbnail = getThumbnail(video["snippet"]["thumbnails"]);
            new_song.duration = convertDuration(video["contentDetails"]["duration"]);
            new_song.type = (song_type)(video["snippet"]["liveBroadcastContent"] != "none");
            new_song.url += video["id"];
        }
    }
    else if (video["title"] != "Deleted video")
    {
        new_song.title = video["title"];
        // livestream
        if (video["duration"] == "")
        {
            new_song.type = livestream;
            new_song.duration = "LIVE";
        }
        else
        {
            new_song.type = video;
            new_song.duration = video["duration"];
        }
        new_song.thumbnail = video["thumbnail"];
        new_song.url += video["id"];
    }
    
    return new_song;
}

void youtube::parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string link, std::string history_entry)
{
    link.erase(0, 1);
    std::string endpoint = YOUTUBE_ENDPOINT;
    size_t mark = link.find('?');
    std::string type = link.substr(0, mark);
    size_t equals = link.find('=', mark);
    size_t aaron = link.find('&', equals);
    std::string id = link.substr(equals+1, (aaron-equals) - 1);
    
    // /playlist?list=<id>
    if (type == "playlist")
    {
        endpoint += "/playlists?part=snippet&id=" + id + "&key=";
    }
    // ?v=<id>
    else if (type == "watch")
    {
        endpoint += "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") + "&id=" + id + "&key=";
    }
    // either /live/<id>? or /<id>?
    else
    {
        id = type;
        size_t slash2 = type.find('/');
        if (slash2 == std::string::npos)
            id = type.substr(slash2+1);
        endpoint += "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") + "&id=" + id + "&key=";
    }
    makeRequest(event, endpoint, history_entry);
}

void youtube::ytsearch(std::pair<dpp::cluster&, dpp::snowflake> event, std::string query, std::string history_entry)
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
        {
            makeRequest(event, std::string(YOUTUBE_ENDPOINT) + "/videos?part=" + dpp::utility::url_encode("snippet,contentDetails") + "&id=" + id + "&key=", history_entry);
        }
    }
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

std::string youtube::getThumbnail(dpp::json& thumbnails)
{
    std::string thumbnail_url;
    if (thumbnails.contains("maxres"))
        thumbnail_url = thumbnails["maxres"]["url"].get<std::string>();
    else if (thumbnails.contains("standard"))
        thumbnail_url = thumbnails["standard"]["url"].get<std::string>();
    else if (thumbnails.contains("high"))
        thumbnail_url = thumbnails["high"]["url"].get<std::string>();
    else if (thumbnails.contains("medium"))
        thumbnail_url = thumbnails["medium"]["url"].get<std::string>();
    else if (thumbnails.contains("default"))
        thumbnail_url = thumbnails["default"]["url"].get<std::string>();
    return thumbnail_url;
}
