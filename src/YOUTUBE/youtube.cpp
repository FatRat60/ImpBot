#include "youtube.h"
#include <iostream>
#include <cstdlib>

// yt-dlp -x --audio-format opus -o test.opus -S +size --no-playlist --force-overwrites --fixup never --extractor-args youtube:player_client=web URL

std::string youtube::YOUTUBE_API_KEY;

bool youtube::isLink(std::string& query)
{
    return query.substr(0, 8).compare("https://") == 0;
}

std::string youtube::pipe_replace(std::string& query)
{
    std::string q;
    size_t cur_ind = 0;
    size_t nex_ind;
    while ( (nex_ind = query.find(' ', cur_ind)) != std::string::npos)
    {
        q += query.substr(cur_ind, nex_ind - cur_ind) + "%2C";
        cur_ind = nex_ind + 1; // start at char to right of space we just found
    }
    q += query.substr(cur_ind, query.length() - cur_ind);
    return q;
}

void youtube::post_search(const dpp::slashcommand_t& event, dpp::json& body)
{
    song youtube_song;
    // You really need to implement JSON here, lazy ass
    if (!body["items"].empty())
    {
        youtube_song.url = YOUTUBE_URL;
        youtube_song.url += body["items"][0]["id"]["videoId"];
        youtube_song.title = body["items"][0]["snippet"]["title"];
    }
}

/*Used by discord bot in /play to download songs*/
void youtube::download(std::string url)
{
    std::string command = "\"yt-dlp\" \"-x\" \"--audio-format\" \"opus\" \"-o\" \"temp/song.opus\" \"-S\" \"+size\" \"--no-playlist\" \"--force-overwrites\" \"--fixup\" \"never\" \"--extractor-args\" \"youtube:player_client=web\" \"" + url + "\"";
    system(command.c_str());
}

void youtube::send_music_buff(dpp::discord_voice_client *voice_client, std::string& song_data, bool add_start_marker)
{
    
}

dpp::embed youtube::create_list_embed(std::string title, std::string footer, std::string contents[10], int num_comp /*=10*/)
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

void youtube::play(dpp::cluster& bot, const dpp::slashcommand_t& event)
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
        std::string search_term = std::get<std::string>(event.get_parameter("link"));
        if (!YOUTUBE_API_KEY.empty()) // Youtube API key exists
        {
            //  TODO parse search term

            // make req
            bot.request(
                std::string(YOUTUBE_ENDPOINT) + std::string("?part=snippet&maxResults=1&type=video&q=") + dpp::utility::url_encode(search_term)  + std::string("&key=") + YOUTUBE_API_KEY,
                dpp::m_get, [event](const dpp::http_request_completion_t& request){
                    if (request.status == 403)
                        event.reply(dpp::message("Rate limited for today. Please try again tmw :("));
                    else if (request.status == 200)
                    {
                        std::cout << request.body << "\n";
                        /*
                        dpp::json body = dpp::json::parse(request.body);
                        youtube::post_search(event, body);*/
                    }
                    else
                        event.reply(dpp::message("There was an error searching youtube. Could not add song"));
                }
            );
        }
        else // API key not found.
        {
            event.reply("Youtube search not available. Tell kyle's dumbass to regenerate the API key");
            return;
        }
    }
}

void youtube::pause(dpp::cluster& bot, const dpp::slashcommand_t& event)
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

void youtube::stop(dpp::cluster& bot, const dpp::slashcommand_t& event)
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

void youtube::skip(dpp::cluster& bot, const dpp::slashcommand_t& event)
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

void youtube::queue(const dpp::slashcommand_t &event)
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

void youtube::remove(const dpp::slashcommand_t &event)
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
                    //songs_to_skip.push(num);
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
