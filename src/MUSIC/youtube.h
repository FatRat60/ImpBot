#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "music_queue.h"
#include <string>
#include <dpp/dpp.h>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3"
#define YOUTUBE_VIDEO_URL "https://www.youtube.com/watch?v="
#define YOUTUBE_LIST_URL "https://www.youtube.com/playlist?list="
#define YUI "resources/yaharo.opus"

struct song_event
{
    dpp::cluster& bot;
    dpp::snowflake guild_id;
    bool shuffle;
    std::string history_entry;

    song_event& zeroHistory()
    {
        this->history_entry = "";
        return *this;
    }

    song_event& appendHistory(std::string str)
    {
        this->history_entry += str;
        return *this;
    }
};

class youtube
{
    public:
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY;}
        static void parseURL(song_event& event, std::string link);
        static void ytsearch(song_event& event, std::string query);
    private:
        static std::string YOUTUBE_API_KEY;
        static std::mutex token_mutex;
        static void makeRequest(song_event& event, std::string endpoint, size_t songs = 0);
        static void handleReply(song_event& event, const dpp::http_request_completion_t& reply, size_t songs);
        static void handleVideo(song_event& event, dpp::json& video);
        static void handlePlaylist(song_event& event, dpp::json& playlist, size_t songs = 0);
        static song createSong(dpp::json& video);
        static std::string convertDuration(std::string old_duration);
        static std::string getThumbnail(dpp::json& thumbnails);
};  

#endif