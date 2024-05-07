#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <string>
#include <dpp/dpp.h>

class youtube
{
    private:
        static std::string YOUTUBE_API_KEY;
    public:
        static bool canSearch() { return YOUTUBE_API_KEY != ""; }
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY; }
        static bool search_video(std::string query);
};  

#endif