#include "discord.h"
#include "youtube.h"
#include <dpp/dpp.h>
#include <oggz/oggz.h>
#include <string>
#include <vector>
#include <unordered_map>

std::unordered_map<std::string, command_name> discord::command_map;

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

        const std::vector<dpp::slashcommand> commands = { pingcmd, joincmd, leavecmd, playcmd };
    
        bot.global_bulk_command_create(commands);
    }

    // populate map
    if (dpp::run_once<struct populate_map>())
    {
        command_map.insert({"ping", PING});
        command_map.insert({"join", JOIN});
        command_map.insert({"leave", LEAVE});
        command_map.insert({"play", PLAY});
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
        }
    }
    else
        event.reply(event.command.get_command_name() + " is not a valid command");
}

void discord::ping(const dpp::slashcommand_t& event)
{
    event.reply("Pong!");
}

void discord::stream_music(dpp::cluster& bot, dpp::discord_voice_client *voice_client)
{
    if (!youtube::isPlaying())
    {
        OGGZ * jeopardy_ogg = oggz_open(JEOPARDY, OGGZ_READ); // open jeopardy file

        oggz_set_read_callback(
            jeopardy_ogg, -1,
            [](OGGZ *oggz, oggz_packet *packet, long serialno,
                void *user_data) {
                    dpp::discord_voice_client *voice_client = (dpp::discord_voice_client *)user_data;

                    voice_client->send_audio_opus(packet->op.packet, packet->op.bytes);
                    return 0;
                },
                (void *)voice_client
        );

        // play jeopardy until req song is rdy to play
        while (!youtube::isPlaying() && !voice_client->terminating)
        {
            static const constexpr long CHUNK_READ = BUFSIZ * 2;

            if (jeopardy_ogg)
            {
                const long read_bytes = oggz_read(jeopardy_ogg, CHUNK_READ);
                if (!read_bytes)
                    oggz_seek(jeopardy_ogg, 0, SEEK_SET); // restart from beginning. I pray this line is NEVER executed
            }
        }
        // close file
        oggz_close(jeopardy_ogg);
    }
    // Now song is ready
    OGGZ *track = oggz_open(youtube::getTrack().c_str(), OGGZ_READ);
    if (!track)
    {
        std::cout << "Error when loading song\n";
        return;
    }
    
    oggz_set_read_callback(
        track, -1,
        [](OGGZ *oggz, oggz_packet *packet, long serialno,
            void *user_data) {
                dpp::discord_voice_client *voice_client = (dpp::discord_voice_client *)user_data;

                voice_client->send_audio_opus(packet->op.packet, packet->op.bytes);
                return 0;
            },
            (void *)voice_client
    );


    while (!voice_client->terminating)
    {
        static const constexpr long CHUNK_READ = BUFSIZ * 2;

        const long read_bytes = oggz_read(track, CHUNK_READ);
        if (!read_bytes) // EOF
            break;
    }

    oggz_close(track);
    youtube::done();
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
                event.reply("Searching for " + url);
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
                }
            }
            else // API key not found. Only links can be used
                event.reply("Youtube search not available. Please provide a link");
        }

        // Now url is a link guaranteed
        if (!youtube::isPlaying())
        {
            event.reply("Playing " + url);
            if(!youtube::download(url))
            {
                event.edit_original_response(dpp::message("could not download music @ " + url));
            }
        }
        else
        {
            // TODO add song to queue
            if (youtube::canPreLoad())
            {
                event.reply("Playing next: " + url);
                if (!youtube::download(url))
                {
                    event.edit_original_response(dpp::message("Could not queue do to failed download"));
                }
            }
            else 
            {
                if (youtube::canQueue())
                {
                    event.reply("Queueing " + url);
                    youtube::queueSong(url);
                }
                else
                {
                    event.reply("Queue is full. Remove songs before trying again");
                }
            }
        }
        if (!join_vc && !youtube::isPlaying())
            stream_music(bot, event.from->get_voice(event.command.guild_id)->voiceclient);
    }
}
    