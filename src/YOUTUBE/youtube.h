#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <string>
#include <dpp/dpp.h>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3/search"

class youtube
{
    private:
        static std::string YOUTUBE_API_KEY;
    public:
        static std::string pipe_replace(std::string& query);
        static std::string getKey() { return YOUTUBE_API_KEY; }
        static bool canSearch() { return !YOUTUBE_API_KEY.empty(); }
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY; }
        static void post_search(dpp::http_request_completion_t result, dpp::cluster& bot);
};  

#endif