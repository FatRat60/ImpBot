#include "discord.h"
#include <dpp/dpp.h>
#include <string>

discord_bot::discord_bot(std::string token) :
    bot(token)
{
    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping")
            event.reply("Pong!");
    });

    bot.on_ready([this](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>())
        {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }
    });
}