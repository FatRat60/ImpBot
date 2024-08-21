#include "discord.h"
#include "youtube.h"
#include "server.h"
#include <oggz/oggz.h>

std::unordered_map<std::string, command_name> discord::command_map;

void discord::register_events(dpp::cluster& bot, const dpp::ready_t& event, bool doRegister, bool doDelete)
{
    // delete commands
    if (doDelete && dpp::run_once<struct delete_bot_commands>())
    {
        bot.log(dpp::loglevel::ll_info, "Deleting Commands");
        bot.global_bulk_command_delete();
    }

    // register commands
    if (doRegister && dpp::run_once<struct register_bot_commands>())
    {
        bot.log(dpp::loglevel::ll_info, "Registering Commands");

        dpp::slashcommand pingcmd("ping", "Ping pong!!!", bot.me.id);
        dpp::slashcommand joincmd("join", "Joins your voice channel", bot.me.id);
        dpp::slashcommand leavecmd("leave", "Leaves the voice channel", bot.me.id);
        dpp::slashcommand playcmd("play", "Play song from a link or a search term", bot.me.id);
        playcmd.add_option(
            dpp::command_option(dpp::co_string, "link", "song link or search term", true)
        );
        dpp::slashcommand pausecmd("pause", "Pause the current song", bot.me.id);
        dpp::slashcommand stopcmd("stop", "stops current song and clears queue", bot.me.id);
        dpp::slashcommand skipcmd("skip", "Skips to the next song", bot.me.id);
        dpp::slashcommand queuecmd("queue", "Displays the current queue", bot.me.id);
        dpp::slashcommand removecmd("remove", "Removes the requested song/songs from queue", bot.me.id);
        removecmd.add_option(
            dpp::command_option(dpp::co_string, "number", "Track number to remove. Seperate by commas and use 1:5 to denote a range", true)
        );
        dpp::slashcommand startcmd("start", "Starts the designated game server", bot.me.id);
        startcmd.add_option(
            dpp::command_option(dpp::co_string, "name", "type of server to start", true).set_auto_complete(true)
        );
        dpp::slashcommand terminatecmd("terminate", "Shuts down the currently active game server", bot.me.id);
        terminatecmd.add_option(
            dpp::command_option(dpp::co_boolean, "restart", "If true, will restart the server after shutdown")
        );
        dpp::slashcommand ipcmd("ip", "I'll slide into your DMs with the server ip <3", bot.me.id);
        dpp::slashcommand shufflecmd("shuffle", "Shuffles the queue", bot.me.id);

        const std::vector<dpp::slashcommand> commands = { 
            pingcmd, joincmd, leavecmd, playcmd,
            pausecmd, stopcmd, skipcmd, queuecmd,
            removecmd, startcmd, terminatecmd, ipcmd,
            shufflecmd
        };
        
    
        bot.global_bulk_command_create(commands);
    }

    // populate map
    if (dpp::run_once<struct populate_map>())
    {
        bot.log(dpp::loglevel::ll_info, "Generating command map");
        command_map.insert({"ping", PING});
        command_map.insert({"join", JOIN});
        command_map.insert({"leave", LEAVE});
        command_map.insert({"play", PLAY});
        command_map.insert({"pause", PAUSE});
        command_map.insert({"stop", STOP});
        command_map.insert({"skip", SKIP});
        command_map.insert({"queue", QUEUE});
        command_map.insert({"remove", REMOVE});
        command_map.insert({"start", START});
        command_map.insert({"terminate", TERMINATE});
        command_map.insert({"ip", IP});
        command_map.insert({"shuffle", SHUFFLE});
    }
}

void discord::handle_slash(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto search = command_map.find(event.command.get_command_name());
    if (search != command_map.end())
    {
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

        case PLAY:
            youtube::play(bot, event);
            break;

        case PAUSE:
            youtube::pause(bot, event);
            break;

        case STOP:
            youtube::stop(bot, event);
            break;

        case SKIP:
            youtube::skip(bot, event);
            break;

        case QUEUE:
            youtube::queue(event);
            break;

        case REMOVE:
            youtube::remove(event);
            break;

        case START:
            server::start(event);
            break;

        case TERMINATE:
            server::terminate(event);
            break;

        case IP:
            server::ip(bot, event);
            break;
        case SHUFFLE:
            youtube::shuffle(event);
            break;
        }
    }
    else
        event.reply(event.command.get_command_name() + " is not a valid command");
}

void discord::ping(const dpp::slashcommand_t& event)
{
    event.reply("Pong!");
}

void discord::hello(dpp::discord_voice_client *voice_client)
{
    OGGZ *yaharo = oggz_open(YUI, OGGZ_READ);
    oggz_set_read_callback(
        yaharo, -1,
        [](OGGZ *oggz, oggz_packet *packet, long serialno,
        void *user_data) {
            dpp::discord_voice_client *voice_client = (dpp::discord_voice_client *)user_data;
            voice_client->send_audio_opus(packet->op.packet, packet->op.bytes);
            return 0;
        },
        (void *)voice_client
    );
    if (yaharo)
    {
        while (!voice_client->terminating)
        {
            const long read_bytes = oggz_read(yaharo, BUFSIZ);
            if (!read_bytes)
                break;
        }

        oggz_close(yaharo);
    }
}

void discord::join(dpp::cluster& bot, const dpp::slashcommand_t& event)
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
        }
        else
            event.reply("Joined your channel!");
    }
    else
        event.reply("Already here, king");
}

void discord::leave(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    // TODO leave will need to properly handle terminating of music 
    auto current_vc = event.from->get_voice(event.command.guild_id);
    if (current_vc)
    {
        youtube::handle_voice_leave(event);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        current_vc->voiceclient->stop_audio();
        event.from->disconnect_voice(event.command.guild_id);
        event.reply("Bye Bye");
    }
    else
        event.reply("Not in a voice channel");
}