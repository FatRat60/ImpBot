#include "discord.h"
#include <oggz/oggz.h>

std::unordered_map<std::string, command_name> discord::command_map;
std::unordered_map<dpp::snowflake, dpp::timer> discord::bot_disconnect_timers;
std::mutex discord::timers_mutex;

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
        ).add_option(
            dpp::command_option(dpp::co_boolean, "shuffle", "whether to shuffle when queueing playlist")
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
            music::play(bot, event);
            break;

        case PAUSE:
            music::pause(bot, event);
            break;

        case STOP:
            music::stop(bot, event);
            break;

        case SKIP:
            music::skip(bot, event);
            break;

        case QUEUE:
            music::queue(event);
            break;

        case REMOVE:
            music::remove(event);
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
            music::shuffle(event);
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

void discord::onSomeoneLeaves(dpp::cluster& bot, const dpp::voice_client_disconnect_t& event)
{
    bot.channel_get(event.voice_client->channel_id,
        [&bot, vc = event.voice_client](const dpp::confirmation_callback_t& callback){
            if (!callback.is_error())
            {
                dpp::channel vc_channel = std::get<dpp::channel>(callback.value);
                // only bot is in vc
                if (vc_channel.is_voice_channel() && vc_channel.get_voice_members().size() == 1)
                {
                    std::lock_guard<std::mutex> guard(timers_mutex);
                    // pause audio
                    if (vc->is_playing())
                        vc->pause_audio(true);
                    dpp::timer timer = bot.start_timer([&bot, vc](const dpp::timer& timer){
                        std::lock_guard<std::mutex> guard(timers_mutex);
                        // remove from map
                        bot_disconnect_timers.erase(vc->channel_id);
                        // discord_client and disconnect from voice
                        dpp::discord_client* from = getDiscordClient(bot, vc->server_id);
                        if (from)
                            from->disconnect_voice(vc->server_id);
                            // stop timer
                        bot.stop_timer(timer);
                    }, 300);
                    // place timer in map
                    bot_disconnect_timers.emplace(vc->channel_id, timer);
                }
            }
        });
}

void discord::onSomeoneTalks(dpp::cluster& bot, const dpp::voice_client_speaking_t& event)
{
    if (bot_disconnect_timers.find(event.voice_client->channel_id) != bot_disconnect_timers.end())
    {
        std::cout << "removing timer\n";
        std::lock_guard<std::mutex> guard(timers_mutex);
        // resume playing
        if (event.voice_client->is_paused())
            event.voice_client->pause_audio(false);
        dpp::timer timer = bot_disconnect_timers.find(event.voice_client->channel_id)->second;
        bot.stop_timer(timer);
        bot_disconnect_timers.erase(event.voice_client->channel_id);
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
    auto voice = event.from->get_voice(event.command.guild_id);
    if (voice)
    {
        event.from->disconnect_voice(event.command.guild_id);
        event.reply("Bye Bye");
    }
    else
        event.reply("Not in a voice channel");
}

dpp::discord_client *discord::getDiscordClient(dpp::cluster &bot, dpp::snowflake guild_id)
{
    const dpp::shard_list& shards = bot.get_shards();
    dpp::discord_client* from = nullptr;
    for (auto shard = shards.begin(); shard != shards.end(); shard++)
    {
        if (shard->second->get_voice(guild_id))
        {
            from = shard->second;
            break;
        }
    }
    return from;
}
