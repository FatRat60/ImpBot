#include <iostream>
#include <string>
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
        discord_bot bot(TOKEN);
        bot.start();
    }
    return 0;
}