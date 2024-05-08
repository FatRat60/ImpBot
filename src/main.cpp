#include <iostream>
#include <string>
#include <dpp/dpp.h>
#include "discord.h"
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

        std::string TOKEN(var);
        dpp::cluster bot(TOKEN, dpp::i_default_intents | dpp::i_message_content);

        bot.on_log(dpp::utility::cout_logger());

        // handle incoming slash commands
        bot.on_slashcommand([&bot](const dpp::slashcommand_t& event){ discord::handle_slash(bot, event); });

        // handle joining voice
        bot.on_voice_ready([&bot](const dpp::voice_ready_t& event){ discord::stream_music(bot, event.voice_client); });

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