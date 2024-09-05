#ifndef MUSIC_H
#define MUSIC_H

#include "youtube.h"
#include "spotify.h"

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
        static void handle_voice_leave(std::pair<dpp::discord_client&, dpp::snowflake> event);
        static void handle_button_press(const dpp::button_click_t& event);
        static void handle_form(dpp::cluster& bot, const dpp::form_submit_t& event);
    private:
        static void parseURL(song_event& event, std::string search_term);
};


#endif