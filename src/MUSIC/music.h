#ifndef MUSIC_H
#define MUSIC_H

#include "youtube.h"
#include "spotify.h"
#include <unordered_map>

class music
{
    public:
        static void play(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void pause(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void stop(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void skip(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void queue(const dpp::slashcommand_t& event);
        static void remove(const dpp::slashcommand_t& event);
        static void shuffle(const dpp::slashcommand_t& event);
        static void handle_marker(const dpp::voice_track_marker_t& marker);
        static void handle_voice_leave(const dpp::slashcommand_t& event);
        static void handle_button_press(const dpp::button_click_t& event);
    private:
        static std::unordered_map<dpp::snowflake, music_queue*> queue_map;
        static std::mutex queue_map_mutex;
        static music_queue* getQueue(const dpp::snowflake guild_id, bool create = false);
};


#endif