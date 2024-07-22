#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <string>
#include <dpp/dpp.h>
#include <thread>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3/search"
#define YOUTUBE_URL "https://www.youtube.com/watch?v="
#define TRACK_FILE "temp/song.opus"
#define YUI "resources/yaharo.opus"

struct song
{
    std::string url;
    std::string title;
    std::string duration;
};

class youtube
{
    private:
        static std::string YOUTUBE_API_KEY;
    public:
        static bool isLink(std::string& query); // returns true if query begins with "https://"
        static std::string pipe_replace(std::string& query);
        static std::string getKey() { return YOUTUBE_API_KEY; }
        static bool canSearch() { return !YOUTUBE_API_KEY.empty(); }
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY; }
        static void post_search(dpp::http_request_completion_t result, song& youtube_song);
        static void download(std::string url);
};  

#endif