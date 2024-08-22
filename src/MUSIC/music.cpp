#include "music.h"
#include "spotify.h"

std::unordered_map<dpp::snowflake, music_queue*> music::queue_map;
std::mutex music::queue_map_mutex;

void music::play(dpp::cluster& bot, const dpp::slashcommand_t& event)
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

    // query music if joined channel AND API key is init'd
    if (doMusic)
    {
        std::string search_term = std::get<std::string>(event.get_parameter("link"));
        event.reply("Searching for " + search_term,
        [event, search_term](const dpp::confirmation_callback_t& callback){
            std::string link = search_term;
            // user sent a link
            music_queue* queue = music::getQueue(event.command.guild_id, true);
            if (search_term.substr(0, 8) == "https://")
            {
                size_t slash = search_term.find('/', 8);
                std::string platform = link.substr(8, slash - 8);
                // music
                if (platform == "www.youtube.com" || platform == "youtube.com" || platform == "youtu.be" || platform == "youtube.music.com")
                {
                    size_t mark = search_term.find('?', slash);
                    std::string type = search_term.substr(slash+1, (mark-slash)-1);
                    size_t equals = search_term.find('=', mark);
                    size_t aaron = search_term.find('&', equals);
                    std::string id = search_term.substr(equals+1, (aaron-equals) - 1);
                    

                    // /playlist?list=<id>
                    if (type == "playlist")
                    {
                        youtube::handle_playlist(event, id, queue);
                    }
                    // ?v=<id>
                    else if (type == "watch")
                    {
                        youtube::handle_video(event, id, queue); 
                    }
                    // either /live/<id>? or /<id>?
                    else
                    {
                        id = type;
                        size_t slash2 = type.find('/');
                        if (slash2 == std::string::npos)
                            id = type.substr(slash2+1);
                        youtube::handle_video(event, id, queue);
                    }
                }
                // spotify
                else if (platform == "open.spotify.com")
                {
                    spotify::parseURL(event, search_term.substr(slash));
                }
                // soundcloud
                else if (platform == "soundcloud.com")
                {
                    event.edit_original_response(dpp::message("Soundcloud support coming soon"));
                }
                else
                    event.edit_original_response(dpp::message("The platform, " + platform + ", is not currently supported"));
            }
            // user sent a query. Search music
            else
            {
                std::string cmd = "yt-dlp -q --no-warnings --flat-playlist --no-playlist --print \"id\" \"ytsearch1:" + search_term + "\"";
                char buffer[50];
                std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
                if (pipe)
                { 
                    fgets(buffer, 50, pipe.get());
                    std::string id = buffer;
                    id.pop_back();
                    youtube::handle_video(event, id, queue);
                }
                else
                    event.edit_original_response(dpp::message("Broken Pipe"));
            }
        });
    }
}

void music::pause(dpp::cluster& bot, const dpp::slashcommand_t& event)
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

void music::stop(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        if (voice_client->is_playing())
        {
            music_queue& queue = *getQueue(event.command.guild_id);
            queue.clear_queue();
            event.reply("Clearing queue...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            voice_client->stop_audio();

            event.edit_original_response(dpp::message("Songs stopped and removed from queue my lord"));
        }
        else
            event.reply("No songs playing");
    }
    else
        event.reply("Jesus man im not in a channel dummy");
}

void music::skip(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    auto voiceconn = event.from->get_voice(event.command.guild_id);
    if (voiceconn)
    {
        auto voice_client = voiceconn->voiceclient;
        music_queue* queue = music::getQueue(voice_client->server_id);
        if (queue && !queue->empty())
        {
            event.reply("Skipped");
            queue->skip(voice_client);
        }
        else
        {
            event.reply("No songs playing");
        }
    }
    else
        event.reply("Tip: join a voice channel before typing /skip");
}

void music::queue(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn) // bot is connected
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue && !queue->empty()) // there is actually music playing
        {
            // send embed
            event.reply(queue->get_queue_embed());
        }
        else
            event.reply("No songs in queue");
    }
    else
        event.reply("Not in a channel pookie. Such a silly billy");
}

void music::remove(const dpp::slashcommand_t &event)
{
    auto voice_conn = event.from->get_voice(event.command.guild_id);
    if (voice_conn)
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue)
        {
            std::string numbers = std::get<std::string>(event.get_parameter("number"));
            // parse start and end
            std::string first, second;
            size_t colon = numbers.find(':');
            if (colon != std::string::npos)
            {
                first = numbers.substr(0, colon);
                second = numbers.substr(colon);
                second.replace(0, 1, "");
            }
            // sanitize them
            if (!first.empty() && isdigit(first[0]))
            {
                size_t start = std::stoul(first);
                size_t end = std::string::npos;
                if (!second.empty())
                {
                    if (isdigit(second[0]))
                        end = std::stoul(second);
                    else
                        end = start+1;
                }
                if (queue->remove_from_queue(start, end))
                {
                    event.reply("removed track(s) from queue");
                }
                else
                    event.reply("could not remove track(s)");
            }
            else
                event.reply("Invalid or non-existant first parameter");
        }
        else
            event.reply("No songs");
    }
    else
        event.reply("Not in voice");
}

music_queue* music::getQueue(const dpp::snowflake guild_id, bool create/* = false */)
{
    std::lock_guard<std::mutex> guard(queue_map_mutex);

    auto res = queue_map.find(guild_id);
    music_queue* found_queue = nullptr;
    if (res == queue_map.end())
    {
        if (create)
        {
            found_queue = new music_queue();
            queue_map.insert({guild_id, found_queue});
        }
    }
    else
        found_queue = res->second;

    return found_queue;
}

void music::handle_marker(const dpp::voice_track_marker_t &marker)
{
    // Checks if marker is to be skipped. If so will invoke skip_to_next_marker()
    // Parse marker for number
    if (marker.track_meta == "end") // found space
    {
        std::thread t([marker]() {
            if (!marker.voice_client->terminating)
            {
                music_queue* queue = music::getQueue(marker.voice_client->server_id);
                if (queue)
                    queue->go_next(marker.voice_client);
            }
        });
        t.detach();
    }
}

void music::handle_voice_leave(const dpp::slashcommand_t& event)
{
    std::unique_ptr<music_queue> queue(getQueue(event.command.guild_id));
    queue_map.erase(event.command.guild_id);
    queue->clear_queue();
}

void music::handle_button_press(const dpp::button_click_t &event)
{
    std::thread t([event](){
        size_t under = event.custom_id.find('_');
        std::string cmd = event.custom_id.substr(0, under);
        // button press to update queue
        if (cmd == "queue")
        {
            dpp::message msg;
            music_queue* queue = getQueue(event.command.guild_id);
            if (queue)
            {
                // refresh
                if (under == std::string::npos)
                    msg = queue->get_queue_embed();
                // prev/next
                else
                {
                    size_t page = queue->getPage();
                    if (event.custom_id.substr(under+1) == "next")
                        page++;
                    else
                        page--;
                    msg = queue->new_page(page);
                }
            }
            else
                msg = dpp::message("Why would you click this knowing there's no music playing? Dumbass...");

            event.reply(dpp::ir_update_message, msg);     
        }
        else if (cmd == "shuffle")
        {
            music_queue* queue = music::getQueue(event.command.guild_id);
            if (queue)
            {
                queue->shuffle();
                event.reply(dpp::ir_update_message, queue->get_queue_embed());
            }   
            else
                event.reply(dpp::ir_update_message, dpp::message("No queue"));
        }
    });
    t.detach();
}

void music::shuffle(const dpp::slashcommand_t &event)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    if (voice)
    {
        music_queue* queue = getQueue(event.command.guild_id);
        if (queue)
        {
            queue->shuffle();
            event.reply("Shuffled!");
        }
        else
            event.reply("Nothing to shuffle dumbass");
    }   
    else
        event.reply("Not in voice");
}
