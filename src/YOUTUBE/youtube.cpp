#include "youtube.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdio.h>

// yt-dlp -q --no-warnings -x --audio-format opus -o - -S +size --no-playlist 
// "yt-dlp -f 140 -q --no-warnings -o - --no-playlist " + youtube_song.url + " | ffmpeg -i pipe:.m4a -c:a libopus -ar 48000 -ac 2 -loglevel quiet pipe:.opus";

std::string youtube::YOUTUBE_API_KEY;
std::unordered_map<dpp::snowflake, music_queue*> youtube::queue_map;
std::mutex youtube::queue_map_mutex;

int youtube::parseLink(std::string& link, std::string& query)
{
    // tenemos un enlace
    if (link.substr(0, 8) == "https://")
    {
        size_t slash = link.find('/', 8);
        std::string platform = link.substr(8, slash - 8);
        // YOUTUBE
        if (platform == "www.youtube.com" || platform == "youtube.com" || platform == "youtu.be")
        {
            size_t mark = link.find('?', slash+1);
            std::string type = link.substr(slash+1, mark - (slash+1));
            if (type == "playlist")
            {
                std::string id = link.substr(link.find('=', mark)+1);
                size_t aaron = id.find('&');
                if (aaron != std::string::npos)
                    id = id.substr(0, aaron);
                query = id;
                return 1;
            }
            else if (type == "watch")
            {
                query = link.substr(link.find('=', mark)+1);
                return 0;
            }
            else
            {
                size_t ind = type.find('/');
                if (ind != std::string::npos)
                    type = type.substr(ind+1);
                query = type;
                return 0;
            }
        }
        else if (platform == "soundcloud.com")
        {
            return 2;
        }
        else
            return 69;
    }
    else
    {
        query = link;
        return 0;
    }
}

void youtube::post_search(const dpp::slashcommand_t& event, const dpp::http_request_completion_t& request)
{
    if (request.status != 200)
    {
        event.reply(request.status == 403 ? "Rate-limited. Try again tmw :(" : "There was an error searching youtube.");
        return;
    }

    dpp::json body = dpp::json::parse(request.body);

    song youtube_song;
    // You really need to implement JSON here, lazy ass
    if (!body["items"].empty())
    {
        youtube_song.url = YOUTUBE_URL;
        youtube_song.url += body["items"][0]["id"]["videoId"];
        youtube_song.title = body["items"][0]["snippet"]["title"];
        youtube_song.thumbnail = body["items"][0]["snippet"]["thumbnails"]["default"]["url"];
        // song type
        if (body["items"][0]["snippet"]["liveBroadcastContent"] == "none")
            youtube_song.type = video;
        else
            youtube_song.type = livestream;

        event.reply("Playing " + youtube_song.url);

        // wait for vc
        dpp::voiceconn* voice = nullptr;
        while ( (voice = event.from->get_voice(event.command.guild_id))->voiceclient == nullptr){}

        // get queue
        music_queue& curr_queue = getQueue(event.command.guild_id);

        if (!curr_queue.enqueue(voice->voiceclient, youtube_song))
            event.edit_original_response(dpp::message("There was an issue.\n" + youtube_song.title + " was not added to queue."));
    }
    else
    {
        event.reply("No songs found from that query");
    }
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
            std::string query;
            int type = parseLink(search_term, query);

            std::string get_url;
            switch (type)
            {
            // youtube - video/livestream
            case 0:
                get_url = std::string(YOUTUBE_ENDPOINT) + "/search?part=snippet&maxResults=1&type=video&q=" + dpp::utility::url_encode(query) + std::string("&key=") + YOUTUBE_API_KEY;
                break;
            
            // youtube - playlist
            case 1:
                //get_url = std::string(YOUTUBE_ENDPOINT) + "/playlists?part=snippet,contentDetails&maxResults=1&id=" + query + std::string("&key=") + YOUTUBE_API_KEY;
                event.reply("Playlists are not supported yet. Only lives or videos");
                return;
                break;

            // soundcloud?
            case 2:
                event.reply("Soundcloud is not yet supported. Plzz stick to youtube links");
                return;
                break;

            // spotify, etc
            default:
                event.reply("This bot does not currently support this platform");
                return;
                break;
            }

            // make req
            bot.request(
                get_url,
                dpp::m_get, [event](const dpp::http_request_completion_t& request){
                    //std::cout << request.body;
                    youtube::post_search(event, request);
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
            std::thread t([voice_client]() {
                music_queue& queue = youtube::getQueue(voice_client->server_id);
                queue.skip(voice_client);
            });
            t.detach();
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

music_queue& youtube::getQueue(const dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> guard(queue_map_mutex);

    auto res = queue_map.find(guild_id);
    music_queue* found_queue;
    if (res == queue_map.end())
    {
        found_queue = new music_queue();
        queue_map.insert({guild_id, found_queue});
    }
    else
        found_queue = res->second;

    return *found_queue;
}

void youtube::handle_marker(const dpp::voice_track_marker_t &marker)
{
    // Checks if marker is to be skipped. If so will invoke skip_to_next_marker()
    // Parse marker for number
    if (marker.track_meta == "end") // found space
    {
        std::cout << "Handling marker\n";
        std::thread t([marker]() {
            music_queue& queue = youtube::getQueue(marker.voice_client->server_id);
            queue.go_next(marker.voice_client);
        });
        t.detach();
    }
}