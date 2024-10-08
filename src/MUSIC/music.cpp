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
        music_queue* queue = music_queue::getQueue(event.command.guild_id, true);
        event.reply(queue->get_embed(), [event](const dpp::confirmation_callback_t& callback){
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

                        // continue to queue
                        song_event song_event = {
                            *event.from->creator, // bot
                            event.command.guild_id, // guild_id
                            true, // shuffle?
                            event.command.get_issuing_user().get_mention(), // history_entry
                            MAX_PLAYLIST_SIZE
                        };
                        try
                        {
                            int num = std::get<int64_t>(event.get_parameter("length"));
                            if (num <= MAX_PLAYLIST_SIZE)
                                song_event.length = num;
                            song_event.shuffle = std::get<bool>(event.get_parameter("shuffle"));
                        }
                        catch(const std::exception& e){}
                        
                        parseURL(song_event, std::get<std::string>(event.get_parameter("link")));
                    }
                });
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
            size_t start, end;
            parseRemove(numbers, start, end);
            // parse start and end
            if (start != std::string::npos)
            {
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
                queue->next();
                music_queue::updateMessage(std::pair<dpp::cluster&, dpp::snowflake>(*marker.voice_client->creator, marker.voice_client->server_id));
            }
        });
        t.detach();
    }
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
            else if (event.custom_id == "add"){
                dpp::interaction_modal_response modal("add", "Please enter Youtube or Spotify link below");
                modal.add_component(
                    dpp::component()
                        .set_label("Please enter Youtube or Spotify link below")
                        .set_id("link")
                        .set_type(dpp::cot_text)
                        .set_placeholder("Your link here")
                        .set_min_length(1)
                        .set_max_length(100)
                        .set_text_style(dpp::text_short)
                )
                .add_row()
                .add_component(
                    dpp::component()
                        .set_label("Optional - How many tracks to queue")
                        .set_id("length")
                        .set_type(dpp::cot_text)
                        .set_default_value("100")
                        .set_min_length(1)
                        .set_max_length(3)
                        .set_text_style(dpp::text_short)
                );

                event.dialog(modal);
                return;
            }
            else if (event.custom_id == "skip")
                doEdit = queue->skip() && queue->getPage() != page_type::history;
            else if (event.custom_id == "next")
                queue->changePageNumber(1);
            else if (event.custom_id == "prev")
                queue->changePageNumber(-1);
            else if (event.custom_id == "remove")
            {
                dpp::interaction_modal_response modal("remove", "Remove Songs");
                modal.add_component(
                    dpp::component()
                        .set_label("Enter track number or provide range")
                        .set_id("num")
                        .set_type(dpp::cot_text)
                        .set_default_value("e.g. 1, 1:4, :5, 1:")
                        .set_min_length(1)
                        .set_max_length(100)
                        .set_text_style(dpp::text_short)
                );
                event.dialog(modal);
                return;
            }
            else if (event.custom_id == "pause"){
                queue->pause();
                doEdit = false;
            }
            else if (event.custom_id == "shuffle"){
                queue->shuffle();
                doEdit = queue->getPage() == page_type::queue;
            }
            else if (event.custom_id == "leave"){
                event.from->disconnect_voice(event.command.guild_id);
                doEdit = false;
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

void music::handle_form(dpp::cluster& bot, const dpp::form_submit_t& event)
{
    if (event.custom_id == "add")
    {
        std::thread t([&bot, event]{
            std::string link = std::get<std::string>(event.components[0].components[0].value);
            std::string length = std::get<std::string>(event.components[1].components[0].value);
            u_int8_t numTracks;
            if (!length.empty() && isdigit(length.front()))
                numTracks = std::stoi(length);
            else
                numTracks = MAX_PLAYLIST_SIZE;
            
            song_event song_event = {
                bot, // bot
                event.command.guild_id, // guild_id
                true, // shuffle?
                event.command.get_issuing_user().get_mention(), // history_entry
                numTracks
            };
            parseURL(song_event, link);
        });
        t.detach();
    }
    else if (event.custom_id == "remove")
    {
        std::thread t([&bot, event](){
            std::string num_str = std::get<std::string>(event.components[0].components[0].value);
            size_t start, end;
            parseRemove(num_str, start, end);
            music_queue* queue = music_queue::getQueue(event.command.guild_id);
            if (queue && start != std::string::npos)
                if (queue->remove_from_queue(start, end))
                    music_queue::updateMessage(std::pair<dpp::cluster&, dpp::snowflake>(bot, event.command.guild_id));
        });
        t.detach();
    }
    event.reply();
}

void music::parseURL(song_event& event, std::string search_term)
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
        else
        {
            music_queue* queue = music_queue::getQueue(event.guild_id);
            if (queue)
                queue->addHistory(event.history_entry + " provided a link that isn't supported...");
        }
    }
    // user sent a query. Search music
    else
    {
        music_queue* queue = music_queue::getQueue(event.guild_id);
        if (queue)
            queue->addHistory(event.history_entry + " searched for " + search_term);
        youtube::ytsearch(event.zeroHistory(), search_term);
    }
}

void music::parseRemove(std::string& num_str, size_t& start, size_t& end)
{
    std::string first, second;
    size_t colon = num_str.find(':');
    if (colon != std::string::npos)
    {
        first = num_str.substr(0, colon);
        second = num_str.substr(colon);
        second.replace(0, 1, "");
        if (!first.empty() && isdigit(first[0]))
            start = std::stoul(first);
        else
            start = 1;
        if (!second.empty())
        {
            if (isdigit(second[0]))
                end = std::stoul(second);
            else
                end = start + 1;
        }
        else
            end = std::string::npos;
    }
    else
    {
        if (!num_str.empty() && isdigit(num_str[0]))
        {
            start = std::stoul(num_str);
            end = start+1;
        }
        else
            start = std::string::npos;
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
