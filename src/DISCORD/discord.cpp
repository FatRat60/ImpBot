#include "discord.h"
#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <unordered_map>

std::unordered_map<std::string, command_name> discord::command_map;

void discord::register_events(dpp::cluster& bot, const dpp::ready_t& event)
{
    if (dpp::run_once<struct register_bot_commands>())
        {
                dpp::slashcommand pingcmd("ping", "Ping pong!!!", bot.me.id);
                dpp::slashcommand joincmd("join", "Joins your voice channel", bot.me.id);
                dpp::slashcommand leavecmd("leave", "Leaves the voice channel", bot.me.id);

                const std::vector<dpp::slashcommand> commands = { pingcmd, joincmd, leavecmd };

                command_map.insert({"ping", PING});
                command_map.insert({"join", JOIN});
                command_map.insert({"leave", LEAVE});
        
                bot.global_bulk_command_create(commands);
        }
}

void discord::handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto search = command_map.find(event.command.get_command_name());
    std::cout << "handling slash\n";
    switch (search->second)
    {
    case PING:
        ping(event);
        break;

    case JOIN:
        join(bot, event);
        break;

    case LEAVE:
        leave(bot, event);
        break;
    
    default:
        event.reply("Not a valid command");
        break;
    }
}

void discord::ping(const dpp::slashcommand_t& event)
{
    event.reply("Pong!");
}

bool discord::join(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    dpp::guild *g = dpp::find_guild(event.command.guild_id);
    auto current_vc = event.from->get_voice(event.command.guild_id);
    bool join_vc = true;

    if (current_vc)
    {
        auto users_vc = g->voice_members.find(event.command.get_issuing_user().id);
        if (users_vc != g->voice_members.end() && current_vc->channel_id != users_vc->second.channel_id)
        {
            event.from->disconnect_voice(event.command.guild_id);
        }
        else 
            join_vc = false;
    }

    if (join_vc)
    {
        if (!g->connect_member_voice(event.command.get_issuing_user().id))
        {
            event.reply("You are not in a voice channel, idiot...");
            join_vc = false;
        }
        else
            event.reply("Joined your channel!");
    }
    else
        event.reply("Already here, king");

    return join_vc;
}

bool discord::leave(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto current_vc = event.from->get_voice(event.command.guild_id);
    if (current_vc)
        event.from->disconnect_voice(event.command.guild_id);
    return current_vc != nullptr;
}