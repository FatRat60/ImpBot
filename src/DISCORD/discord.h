#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>
#include <string>
#include <unordered_map>
#include <queue>
#include <thread>

#define MAX_EMBED_VALUES 10

enum command_name
{
    PING,
    JOIN,
    LEAVE,
    PLAY,
    PAUSE,
    STOP,
    SKIP,
    QUEUE,
    REMOVE,
    START,
    TERMINATE,
    IP
};

class discord
{
    public:
        static void handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void register_events(dpp::cluster& bot, const dpp::ready_t& event, bool doRegister, bool doDelete);
        static void send_music_buff(dpp::discord_voice_client *voice_client, std::string& song_data, bool add_start_marker);
        static void hello(dpp::discord_voice_client *voice_client);
        static void handle_marker(const dpp::voice_track_marker_t& marker);
        static dpp::embed create_list_embed(std::string title, std::string footer, std::string contents[10], int num_comp);
    private:
        static std::unordered_map<std::string, command_name> command_map; // map commands to command name enum
        static std::queue<size_t> songs_to_skip; // Matches songs to skip to a guild id
        static std::thread download_thread;
        static void ping(const dpp::slashcommand_t& event);
        static void join(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void leave(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void play(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void pause(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void stop(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void skip(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void queue(const dpp::slashcommand_t& event);
        static void remove(const dpp::slashcommand_t& event);
};

#endif