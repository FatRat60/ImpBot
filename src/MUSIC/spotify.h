#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <string>
#include <dpp/dpp.h>

#define SPOTIFY_ENDPOINT "https://api.spotify.com/v1"

class spotify
{
    public:
        static void setSpotifyId(std::string CLIENT_ID, std::string CLIENT_SECRET){ SPOTIFY_CLIENT_ID = CLIENT_ID; SPOTIFY_CLIENT_SECRET = CLIENT_SECRET;}
        static void parseURL(const dpp::slashcommand_t& event, std::string url);
    private:
        static std::mutex token_mutex;
        static std::string SPOTIFY_CLIENT_ID;
        static std::string SPOTIFY_CLIENT_SECRET;
        static std::string SPOTIFY_ACCESS_TOKEN;
        static std::string SPOTIFY_REFRESH_TOKEN;
        static std::string hasAccessToken(const dpp::slashcommand_t& event, std::string& endpoint);
};

#endif