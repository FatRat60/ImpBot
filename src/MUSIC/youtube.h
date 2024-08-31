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
        static void parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string link);
        static void ytsearch(std::pair<dpp::cluster&, dpp::snowflake> event, std::string query, bool doReply = true);
    private:
        static std::string YOUTUBE_API_KEY;
        static std::mutex token_mutex;
        static void makeRequest(std::pair<dpp::cluster&, dpp::snowflake> event, std::string endpoint, bool doReply = true, size_t songs = 0);
        static void handleReply(std::pair<dpp::cluster&, dpp::snowflake> event, const dpp::http_request_completion_t& reply, bool doReply, size_t songs);
        static void handleVideo(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& video, bool doReply);
        static void handlePlaylist(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, bool doReply, size_t songs = 0);
        static song createSong(dpp::json& video);
        static std::string convertDuration(std::string old_duration);
        static std::string getThumbnail(dpp::json& thumbnails);
};  

#endif