#include <iostream>
#include <string>
#include <dpp/dpp.h>
#include "discord.h"
#include "server.h"
#include "youtube.h"
extern "C" {
    #include "parseENV.h"
}

#define PARAM_LEN 4

int main(int argc, char *argv[])
{
    parse_env();
    char * var;
    if ( (var = get_env_var("DISCORD_TOKEN")) != NULL)
    {
        // try to init youtube api
        char *var2 = get_env_var("YOUTUBE_API_KEY");
        if (var2) 
            youtube::setAPIkey(std::string(var2));

        // init server dir
        var2 = get_env_var("SERVER_BASE_DIR");
        if (var2)
            server::setDir(std::string(var2));
            
        std::string TOKEN(var);
        dpp::cluster bot(TOKEN, dpp::i_default_intents | dpp::i_message_content);

        bot.on_log(dpp::utility::cout_logger());

        // check for active server
        server::checkForActiveServer(bot);

        // handle incoming slash commands
        bot.on_slashcommand([&bot](const dpp::slashcommand_t& event){ discord::handle_slash(bot, event); });

        // handle joining voice
        bot.on_voice_ready([&bot](const dpp::voice_ready_t& event){
            auto vc = event.voice_client;
            if (vc)
            {
                discord::hello(vc); // bot greeting
                std::string url; // first song wont appear in queue so no url needed
                discord::send_music_buff(vc, url, false);
            }
        });

        // handle passing marker in audio
        bot.on_voice_track_marker([](const dpp::voice_track_marker_t& marker){ discord::handle_marker(marker); });

        // handle autocomplete
        bot.on_autocomplete([&bot](const dpp::autocomplete_t& event) {
            for (auto& opt : event.options)
            {
                if (opt.focused)
                {
                    std::string val = std::get<std::string>(opt.value);
                    dpp::interaction_response response(dpp::ir_autocomplete_reply);

                    std::vector<std::string> dirs = server::get_available_servers();
                    for (std::string dir : dirs)
                        response.add_autocomplete_choice(
                            dpp::command_option_choice(dir, dir)
                        );

                    bot.interaction_response_create(event.command.id, event.command.token, response);
                    break;
                }
            }
        });

        // register events
        bot.on_ready([&bot, &argc, &argv](const dpp::ready_t& event){ 
            // parse command line to find out if bot needs to register/unregister commands
            bool doRegister = argc > 1 && strncmp(argv[1], "-reg", PARAM_LEN) == 0;
            bool doDelete = argc > 1 + doRegister && strncmp(argv[1+doRegister], "-del", PARAM_LEN) == 0;
            discord::register_events(bot, event, doRegister, doDelete); 
        });

        bot.start(dpp::st_wait);
    }
    return 0;
}