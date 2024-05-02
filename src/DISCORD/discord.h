#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>
#include <string>

class discord_bot
{
    public:
        discord_bot(std::string token);
        void start() { bot.start(dpp::st_wait); }

    private:
        dpp::cluster bot;
};

#endif