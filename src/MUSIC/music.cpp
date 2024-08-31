#include "music.h"
#include "spotify.h"

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
        // create queue obj
        music_queue::getQueue(event.command.guild_id, true);
        event.reply("Getting player embed...", [event](const dpp::confirmation_callback_t& callback){
            music_queue* queue = music_queue::getQueue(event.command.guild_id);
            if (!callback.is_error() && queue)
            {
                dpp::message* cur_msg = music_queue::getMessage(queue->getPlayerID());
                // invoked play again, remove old one if exists and cache new one
                if (cur_msg)
                {
                    music_queue::removeMessage(queue->getPlayerID());
                    event.from->creator->message_delete(cur_msg->id, cur_msg->channel_id);
                    cur_msg = nullptr;
                }
                // cache new one
                event.get_original_response([event](const dpp::confirmation_callback_t& callback){
                    music_queue* queue = music_queue::getQueue(event.command.guild_id);
                    if (!callback.is_error() && queue)
                    {
                        // cache msg and set id
                        dpp::message new_msg = std::get<dpp::message>(callback.value);
                        music_queue::cacheMessage(new_msg);
                        queue->setPlayerID(new_msg.id);
                    }
                });
                // continue to queue
                std::pair<dpp::cluster&, dpp::snowflake> pair(*event.from->creator, event.command.guild_id);
                parseURL(pair, std::get<std::string>(event.get_parameter("link")));
            }
            else
                std::cout << "this better not print\n";
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
    music_queue* queue = music_queue::getQueue(event.command.guild_id);
    if (queue)
    {
        if (!queue->empty())
        {
            music_queue* queue = music_queue::getQueue(event.command.guild_id);
            queue->clear_queue();
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
    music_queue* queue = music_queue::getQueue(event.command.guild_id);
    if (queue)
    {
        if (!queue->empty())
        {
        event.reply("Skipped");
        if (queue->skip())
            music_queue::updateMessage(std::pair<dpp::cluster&, dpp::snowflake>(*event.from->creator, event.command.guild_id));
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
        music_queue* queue = music_queue::getQueue(event.command.guild_id);
        if (queue && !queue->empty()) // there is actually music playing
        {
            // send embed
            event.reply(queue->get_embed());
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
        music_queue* queue = music_queue::getQueue(event.command.guild_id);
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

void music::handle_marker(const dpp::voice_track_marker_t &marker)
{
    // Checks if marker is to be skipped. If so will invoke skip_to_next_marker()
    // Parse marker for number
    if (marker.track_meta == "end") // found space
    {
        std::thread t([marker]() {
            music_queue* queue = music_queue::getQueue(marker.voice_client->server_id);
            if (queue)
            {
                queue->go_next();
                music_queue::updateMessage(std::pair<dpp::cluster&, dpp::snowflake>(*marker.voice_client->creator, marker.voice_client->server_id));
            }
        });
        t.detach();
    }
}

void music::handle_voice_leave(const dpp::slashcommand_t& event)
{
    music_queue::removeQueue(event.command.guild_id);
}

void music::handle_button_press(const dpp::button_click_t &event)
{
    std::thread t([event](){
        music_queue* queue = music_queue::getQueue(event.command.guild_id);
        if (queue)
        {
            // parse commands
            bool doEdit = true;
            if (event.custom_id == "play")
                queue->setPage(page_type::playback_control);
            else if (event.custom_id == "queue")
                queue->setPage(page_type::queue);
            else if (event.custom_id == "history")
                queue->setPage(page_type::history);
            else if (event.custom_id == "add"){}
                // TODO fire the modal
            else if (event.custom_id == "skip")
                doEdit = queue->skip() && queue->getPage() != page_type::history;
            else if (event.custom_id == "next")
                queue->changePageNumber(1);
            else if (event.custom_id == "prev")
                queue->changePageNumber(-1);
            else if (event.custom_id == "pause"){
                queue->pause();
                doEdit = false;
            }
            else if (event.custom_id == "shuffle"){
                queue->shuffle();
                doEdit = queue->getPage() == page_type::queue;
            }
            else if (event.custom_id == "stop"){
                queue->clear_queue();
                doEdit = queue->getPage() != page_type::history;
            }
            else
                doEdit = false;
            if (doEdit)
                event.reply(dpp::ir_update_message, queue->get_embed());
            else
                event.reply();
        }
        else
            event.reply(dpp::ir_update_message, dpp::message("Why would you click this knowing there's no music playing? Dumbass..."));
    });
    t.detach();
}

void music::parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string search_term)
{
    if (search_term.substr(0, 8) == "https://")
    {
        size_t slash = search_term.find('/', 8);
        std::string platform = search_term.substr(8, slash - 8);
        // music
        if (platform == "www.youtube.com" || platform == "youtube.com" || platform == "youtu.be" || platform == "music.youtube.com")
        {
            youtube::parseURL(event, search_term.substr(slash));
        }
        // spotify
        else if (platform == "open.spotify.com")
        {
            spotify::parseURL(event, search_term.substr(slash));
        }
        // soundcloud
        else if (platform == "soundcloud.com")
        {
            //event.edit_original_response(dpp::message("Soundcloud support coming soon"));
        }
        //else
            //event.edit_original_response(dpp::message("The platform, " + platform + ", is not currently supported"));
    }
    // user sent a query. Search music
    else
    {
        youtube::ytsearch(event, search_term);
    }
}

void music::shuffle(const dpp::slashcommand_t& event)
{
    auto voice = event.from->get_voice(event.command.guild_id);
    if (voice)
    {
        music_queue* queue = music_queue::getQueue(event.command.guild_id);
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
