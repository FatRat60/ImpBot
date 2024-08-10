#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>
#include <string>
#include <unordered_map>
#include <queue>
#include <thread>

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
        static void hello(dpp::discord_voice_client *voice_client);
        
    private:
        static std::unordered_map<std::string, command_name> command_map; // map commands to command name enum
        static void ping(const dpp::slashcommand_t& event);
        static void join(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void leave(dpp::cluster& bot, const dpp::slashcommand_t& event);
};

#endif