#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "music_queue.h"
#include <string>
#include <dpp/dpp.h>
#include <unordered_map>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3"
#define YOUTUBE_VIDEO_URL "https://www.youtube.com/watch?v="
#define YOUTUBE_LIST_URL "https://www.youtube.com/playlist?list="
#define YUI "resources/yaharo.opus"

class youtube
{
    public:
        static void play(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void pause(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void stop(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void skip(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void queue(const dpp::slashcommand_t& event);
        static void remove(const dpp::slashcommand_t& event);
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY;}
        static void handle_marker(const dpp::voice_track_marker_t& marker);
        static void handle_voice_leave(const dpp::slashcommand_t& event);
        static void handle_button_press(const dpp::button_click_t& event);
    private:
        static std::string YOUTUBE_API_KEY;
        static std::unordered_map<dpp::snowflake, music_queue*> queue_map;
        static std::unordered_map<dpp::snowflake, dpp::timer> timer_map;
        static std::mutex queue_map_mutex;
        static std::mutex timer_map_mutex;
        static song get_song_info(std::string& query);
        static void handle_video(const dpp::slashcommand_t& event, std::string query);
        static void handle_playlist(const dpp::slashcommand_t& event, std::string query);
        static music_queue* getQueue(const dpp::snowflake guild_id, bool create = false);
};  

#endif