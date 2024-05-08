#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>
#include <string>
#include <unordered_map>

enum command_name
{
    PING,
    JOIN,
    LEAVE,
    PLAY
};

class discord
{
    public:
        static void handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void register_events(dpp::cluster& bot, const dpp::ready_t& event, bool doRegister, bool doDelete);
        static void stream_music(dpp::cluster& bot, dpp::discord_voice_client *voice_client);
    private:
        static std::unordered_map<std::string, command_name> command_map;
        static void ping(const dpp::slashcommand_t& event);
        static void join(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void leave(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void play(dpp::cluster& bot, const dpp::slashcommand_t& event);
};

#endif