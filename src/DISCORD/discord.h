#ifndef DISCORD_H
#define DISCORD_H

#include "music.h"
#include "server.h"
#include <dpp/dpp.h>
#include <string>
#include <unordered_map>
#include <queue>
#include <thread>

#define TEAM_TOBY 388158533004165121

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
    IP,
    SHUFFLE
};

class discord
{
    public:
        static void handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void register_events(dpp::cluster& bot, const dpp::ready_t& event, bool doRegister, bool doDelete);
        static void hello(dpp::discord_voice_client *voice_client);
        static void onSomeoneLeaves(dpp::cluster& bot, const dpp::voice_client_disconnect_t& event);
        static void onSomeoneTalks(dpp::cluster& bot, const dpp::voice_client_speaking_t& event);
        
    private:
        static std::unordered_map<std::string, command_name> command_map; // map commands to command name enum
        static std::unordered_map<dpp::snowflake, dpp::timer> bot_disconnect_timers;
        static std::mutex timers_mutex;
        static void ping(const dpp::slashcommand_t& event);
        static void join(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void leave(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static dpp::discord_client* getDiscordClient(dpp::cluster& bot, dpp::snowflake guild_id);
};

#endif