#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>
#include <string>
#include <unordered_map>

enum command_name
{
    PING,
    JOIN,
    LEAVE
};

class discord
{
    public:
        static void handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void register_events(dpp::cluster& bot, const dpp::ready_t& event);
    private:
        static std::unordered_map<std::string, command_name> command_map;
        static void ping(const dpp::slashcommand_t& event);
        static bool join(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static bool leave(dpp::cluster& bot, const dpp::slashcommand_t& event);
};

#endif