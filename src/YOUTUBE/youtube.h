#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <string>
#include <dpp/dpp.h>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3/search"
#define YOUTUBE_URL "https://www.youtube.com/watch?v="
#define JEOPARDY "resources/jeopardy.opus"
#define MAX_QUEUE_SIZE 30

class youtube
{
    private:
        static std::string YOUTUBE_API_KEY;
        static const std::string music_files[];
        static int current_track; // either 0 or 1. Will be -1 if no track is playing
        static size_t queue_size; // 0 - 30
        static std::string queue[MAX_QUEUE_SIZE]; // holds url for queued songs
        static bool preloaded; // true if track is preloaded
    public:
        static const std::string& getTrack() { return music_files[current_track]; }
        static void done() { current_track = -1; queue_size = 0; preloaded = 0; } // resets class to no music values
        static void queueSong(std::string& url) { queue[queue_size++] = url; } // adds url to queue and inc queue size
        static bool canQueue() { return queue_size == MAX_QUEUE_SIZE; } // returns true if queue not full
        static bool canPreLoad() { return !preloaded; } // returns true if no track is preloaded
        static bool isLink(std::string& query); // returns true if query begins with "https://"
        static std::string pipe_replace(std::string& query);
        static std::string getKey() { return YOUTUBE_API_KEY; }
        static bool canSearch() { return !YOUTUBE_API_KEY.empty(); }
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY; }
        static std::string post_search(dpp::http_request_completion_t result);
        static bool download(std::string& url);
        static bool isPlaying() { return current_track != -1; }
};  

#endif