#include "discord.h"
#include "youtube.h"
#include <oggz/oggz.h>
#include <vector>

std::unordered_map<std::string, command_name> discord::command_map;
std::thread discord::download_thread;

void discord::register_events(dpp::cluster& bot, const dpp::ready_t& event, bool doRegister, bool doDelete)
{
    // delete commands
    if (doDelete && dpp::run_once<struct delete_bot_commands>())
    {
        bot.global_bulk_command_delete();
    }

    // register commands
    if (doRegister && dpp::run_once<struct register_bot_commands>())
    {
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

        const std::vector<dpp::slashcommand> commands = { 
            pingcmd, joincmd, leavecmd, playcmd,
            pausecmd, stopcmd, skipcmd
         };
    
        bot.global_bulk_command_create(commands);
    }

    // populate map
    if (dpp::run_once<struct populate_map>())
    {
        command_map.insert({"ping", PING});
        command_map.insert({"join", JOIN});
        command_map.insert({"leave", LEAVE});
        command_map.insert({"play", PLAY});
        command_map.insert({"pause", PAUSE});
        command_map.insert({"stop", STOP});
        command_map.insert({"skip", SKIP});
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
            play(bot, event);
            break;

        case PAUSE:
            pause(bot, event);
            break;

        case STOP:
            stop(bot, event);
            break;

        case SKIP:
            skip(bot, event);
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

void discord::send_music_buff(dpp::cluster& bot, dpp::discord_voice_client *voice_client)
{
    // wait for download to finish
    if (download_thread.joinable())
        download_thread.join();

    OGGZ * ogg = oggz_open(TRACK_FILE, OGGZ_READ);
    oggz_set_read_callback(
        ogg, -1,
        [](OGGZ *oggz, oggz_packet *packet, long serialno,
        void *user_data) {
            dpp::discord_voice_client *voice_client = (dpp::discord_voice_client *)user_data;
            voice_client->send_audio_opus(packet->op.packet, packet->op.bytes);
            return 0;
        },
        (void *)voice_client
    );

    if (ogg) // file opened. Send the audio
    {

        if (voice_client->is_playing()) // song already in buffer
            voice_client->insert_marker(); // marker indicating new song

        while (!voice_client->terminating) 
        {
            static const constexpr long CHUNK_READ = BUFSIZ * 2;
            const long read_bytes = oggz_read(ogg, CHUNK_READ);
            if (!read_bytes)
                break;
        }

        oggz_close(ogg); // close ogg file
        remove(TRACK_FILE); // remove tmp file
    }
    else
        bot.message_create(dpp::message(voice_client->channel_id, "Download Failed"));
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
        voice_client->send_silence(20);

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
        event.from->disconnect_voice(event.command.guild_id);
        event.reply("Bye Bye");
    }
    else
        event.reply("Not in a voice channel");
}

void discord::play(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    // join voice channel first
    dpp::guild *g = dpp::find_guild(event.command.guild_id);
    auto current_vc = event.from->get_voice(event.command.guild_id);
    bool join_vc = true;
    bool doMusic = true;

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
            doMusic = false;
        }
    }

    // query youtube if joined channel AND API key is init'd
    if (doMusic)
    {
        std::string url = std::get<std::string>(event.get_parameter("link"));
        if (!youtube::isLink(url)) // have to search using api
        {
            if (youtube::canSearch()) // Youtube API key exists
            {
                // make req
                dpp::http_request req(
                    std::string(YOUTUBE_ENDPOINT) + std::string("?part=snippet&maxResults=1&type=video&q=") + youtube::pipe_replace(url) + std::string("&key=") + youtube::getKey(),
                    nullptr,
                    dpp::m_get
                );
                dpp::http_request_completion_t result = req.run(&bot);

                // parse response
                if (result.status == 200)
                {
                    url = youtube::post_search(result);
                    if (url.empty())
                    {
                        event.reply("No results found. Please try again");
                        return;
                    }
                }
                else
                {
                    event.reply("Authorization failed. Regenerate API key");
                    return;
                }
            }
            else // API key not found. Only links can be used
                event.reply("Youtube search not available. Please provide a link");
                return;
        }

        // download url
        std::cout << "joinable: " << download_thread.joinable();
        while (download_thread.joinable()){} // thread hasnt been joined by send_music_buff yet
        std::cout << "Garsh\n";
        download_thread = std::thread(youtube::download, url); // launch thread

        if (!join_vc) // manually call send_music_buff since on_voice_ready wont trigger
        {
            if (current_vc->voiceclient->get_tracks_remaining() > 1)
                event.reply("Queueing " + url);
            else
                event.reply("Playing next: " + url);
            send_music_buff(bot, current_vc->voiceclient);
        }
        else // on_voice_ready will invoke send_music_buff
        {
            event.reply("Playing " + url);
        }
    }
}

void discord::pause(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            bool paused = voice_client->is_paused();
            voice_client->pause_audio(!paused);
            std::string append = paused ? "resumed" : "paused";
            event.reply("Song " + append);
        }
        else
        {
            event.reply("No song playing");
        }
    }
    else
    {
        event.reply("Im not in a voice channel IDIOT");
    }
}

void discord::stop(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            voice_client->stop_audio();
            event.reply("Songs stopped and removed from queue my lord");
        }
        else
            event.reply("No songs playing");
    }
    else
        event.reply("Jesus man im not in a channel dummy");
}

void discord::skip(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            event.reply("Skipped");
            voice_client->skip_to_next_marker();
        }
        else
        {
            event.reply("No songs playing");
        }
    }
    else
        event.reply("Tip: join a voice channel before typing /skip");
}