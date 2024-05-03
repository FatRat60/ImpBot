#include <iostream>
#include <string>
#include <dpp/dpp.h>
#include "DISCORD/discord.h"
extern "C" {
    #include "ENV/parseENV.h"
}

int main(int, char**)
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
        bot.on_ready([&bot](const dpp::ready_t& event){ discord::register_events(bot, event); });

        bot.start(dpp::st_wait);
    }
    return 0;
}