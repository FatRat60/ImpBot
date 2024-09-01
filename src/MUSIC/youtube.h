#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "music_queue.h"
#include <string>
#include <dpp/dpp.h>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3"
#define YOUTUBE_VIDEO_URL "https://www.youtube.com/watch?v="
#define YOUTUBE_LIST_URL "https://www.youtube.com/playlist?list="
#define YUI "resources/yaharo.opus"

class youtube
{
    public:
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY;}
        static void parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string link, std::string history_entry = "");
        static void ytsearch(std::pair<dpp::cluster&, dpp::snowflake> event, std::string query, std::string history_entry = "");
    private:
        static std::string YOUTUBE_API_KEY;
        static std::mutex token_mutex;
        static void makeRequest(std::pair<dpp::cluster&, dpp::snowflake> event, std::string endpoint, std::string history_entry = "", size_t songs = 0);
        static void handleReply(std::pair<dpp::cluster&, dpp::snowflake> event, const dpp::http_request_completion_t& reply, std::string history_entry, size_t songs);
        static void handleVideo(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& video, std::string history_entry);
        static void handlePlaylist(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, std::string history_entry, size_t songs = 0);
        static song createSong(dpp::json& video);
        static std::string convertDuration(std::string old_duration);
        static std::string getThumbnail(dpp::json& thumbnails);
};  

#endif