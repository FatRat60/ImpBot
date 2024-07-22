#include "discord.h"
#include "youtube.h"
#include <oggz/oggz.h>

std::unordered_map<std::string, command_name> discord::command_map;
std::queue<size_t> discord::songs_to_skip;
std::thread discord::download_thread;

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

        const std::vector<dpp::slashcommand> commands = { 
            pingcmd, joincmd, leavecmd, playcmd,
            pausecmd, stopcmd, skipcmd, queuecmd,
            removecmd
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

        case QUEUE:
            queue(event);
            break;

        case REMOVE:
            remove(event);
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

void discord::send_music_buff(dpp::discord_voice_client *voice_client, std::string& song_data, bool add_start_marker)
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

        if (add_start_marker) // song already in buffer FIGURE OUT THIS
        {
            std::string track = std::to_string((voice_client->get_tracks_remaining() / 2) - songs_to_skip.size());
            voice_client->insert_marker(track + " " + song_data); // marker indicating new song in form "trackNo url"
            voice_client->log(dpp::loglevel::ll_info, "Track " + track + " has been queued");
        }

        while (!voice_client->terminating) 
        {
            static const constexpr long CHUNK_READ = BUFSIZ * 2;
            const long read_bytes = oggz_read(ogg, CHUNK_READ);
            if (!read_bytes)
                break;
        }

        voice_client->insert_marker("");

        oggz_close(ogg); // close ogg file
        std::remove(TRACK_FILE); // remove tmp file
    }
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

void discord::handle_marker(const dpp::voice_track_marker_t &marker)
{
    // Checks if marker is to be skipped. If so will invoke skip_to_next_marker()
    // Parse marker for number
    size_t pos = marker.track_meta.find(' ');
    if (pos != std::string::npos) // found space
    {
        std::cout << "Handling marker\n";
        // parse metadata for track number
        std::string track_num_as_str = marker.track_meta.substr(0, pos);
        size_t track_num = std::stol(track_num_as_str);
        if (songs_to_skip.front() == track_num) // Dont play song because it got removed
        {
            if (marker.voice_client != nullptr)
            {
                marker.voice_client->skip_to_next_marker();
                marker.voice_client->creator->log(dpp::loglevel::ll_info, "Song skipped successfully");
            }
            else
                marker.voice_client->creator->log(dpp::loglevel::ll_warning, "No voice client. Could not skip");
            songs_to_skip.pop(); // pop off queue
        }
    } 
    else
    {
        if (!marker.track_meta.empty())
            marker.voice_client->creator->log(dpp::loglevel::ll_warning, std::string("Marker could not be parsed correctly\n\tMetadata: " + marker.track_meta));
    }
}

dpp::embed discord::create_list_embed(std::string title, std::string footer, std::string contents[10], int num_comp /*=10*/)
{
    // init embed with default vals
    dpp::embed embed = dpp::embed()
        .set_color(dpp::colors::pink_rose)
        .set_title(title)
        .set_footer(
            dpp::embed_footer()
            .set_text(footer)
        )
        .set_timestamp(time(0));

    // loop through contents and add fields
    for (int i = 0; i < num_comp; i++)
    {
        embed.add_field(std::to_string(i+1) + ". ",
            contents[i]);
    }

    // return new embed
    return embed;
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
        song youtube_song;
        std::string search_term = std::get<std::string>(event.get_parameter("link"));
        if (youtube::canSearch()) // Youtube API key exists
        {
            // make req
            dpp::http_request req(
                std::string(YOUTUBE_ENDPOINT) + std::string("?part=snippet&maxResults=1&type=video&q=") + youtube::pipe_replace(search_term) + std::string("&key=") + youtube::getKey(),
                nullptr,
                dpp::m_get
            );
            dpp::http_request_completion_t result = req.run(&bot);
            // parse response
            if (result.status == 200)
            {
                youtube::post_search(result, youtube_song);
                if (youtube_song.url.empty())
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
        {
            event.reply("Youtube search not available. Tell kyle's dumbass to regenerate the API key");
            return;
        }
        bot.log(dpp::loglevel::ll_info, "Start Download");

        // download url
        while (download_thread.joinable()){} // thread hasnt been joined by send_music_buff yet
        download_thread = std::thread(youtube::download, youtube_song.url); // launch thread

        if (!join_vc)
        {
            if (current_vc->voiceclient->get_tracks_remaining() > 1)
                event.reply("Queueing " + youtube_song.url);
            else
                event.reply("Playing next: " + youtube_song.url);
            send_music_buff(current_vc->voiceclient, youtube_song.title, current_vc->voiceclient->is_playing());
        }
        else // wait for voice to connect
        {
            event.reply("Playing " + youtube_song.url);
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

void discord::queue(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn) // bot is connected
    {
        if (voice_conn->voiceclient->get_tracks_remaining() > 2) // there is actually music playing
        {
            const std::vector<std::string>& metadata = voice_conn->voiceclient->get_marker_metadata(); // get metadata
            std::string contents[MAX_EMBED_VALUES];
            int num_songs = 0;
            // create embed of queue
            for (std::string marker : metadata)
            {
                if (marker.empty()) // Marker denotes end of song and should not be counted
                    continue;
                
                // get marker data
                if (num_songs < MAX_EMBED_VALUES)
                {
                    size_t pos = marker.find(' ');
                    if (pos == std::string::npos) // poor marker data
                        continue;
                    std::string song_data = marker.substr(pos+1);
                    contents[num_songs] = song_data;
                }

                // inc num songs
                num_songs++;
            }
            // send embed
            dpp::embed queue_embed = create_list_embed("Queue", std::to_string(num_songs) + " songs", contents, num_songs < MAX_EMBED_VALUES ? num_songs : MAX_EMBED_VALUES);
            event.reply(dpp::message(event.command.channel_id, queue_embed));
        }
        else
            event.reply("No songs in queue");
    }
    else
        event.reply("Not in a channel pookie. Such a silly billy");
}

void discord::remove(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn)
    {
        if (voice_conn->voiceclient->is_playing())
        {
            std::string numbers = std::get<std::string>(event.get_parameter("number"));
            if (!numbers.empty())
            {
                size_t num = std::stol(numbers.substr(0, numbers.size()));
                if (num < voice_conn->voiceclient->get_tracks_remaining()) // number to remove is within the range of songs queued
                {
                    // try to add to map
                    songs_to_skip.push(num);
                    event.from->log(dpp::loglevel::ll_info, numbers + " to be skipped");
                    event.reply("Track " + numbers + " was removed from queue");
                }
                else
                    event.reply("Could not remove track " + numbers.substr(0, numbers.size()) + ". Does not exist");
            }
        }
        else
            event.reply("No songs ");
    }
    else
        event.reply("Not in voice");
}
