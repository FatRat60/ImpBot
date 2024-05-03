#include <iostream>
#include <string>
#include <dpp/dpp.h>
#include "DISCORD/discord.h"
extern "C" {
    #include "ENV/parseENV.h"
}

#define PARAM_LEN 4

int main(int argc, char *argv[])
{
    parse_env();
    char * var;
    if ( (var = get_env_var("DISCORD_TOKEN")) != NULL)
    {
        std::string TOKEN(var);
        dpp::cluster bot(TOKEN, dpp::i_default_intents | dpp::i_message_content);

        bot.on_log(dpp::utility::cout_logger());

        // handle incoming slash commands
        bot.on_slashcommand([&bot](const dpp::slashcommand_t& event){ discord::handle_slash(bot, event); });

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