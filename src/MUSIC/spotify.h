#ifndef SPOTIFY_H
#define SPOTIFY_H

#include "youtube.h"
#include <string>
#include <dpp/dpp.h>

#define SPOTIFY_ENDPOINT "https://api.spotify.com/v1"

class spotify
{
    public:
        static void setSpotifyId(std::string CLIENT_ID, std::string CLIENT_SECRET){ SPOTIFY_CLIENT_ID = CLIENT_ID; SPOTIFY_CLIENT_SECRET = CLIENT_SECRET;}
        static void parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string url, std::string history_entry = "");
    private:
        static std::mutex token_mutex;
        static std::string SPOTIFY_CLIENT_ID;
        static std::string SPOTIFY_CLIENT_SECRET;
        static std::string SPOTIFY_ACCESS_TOKEN;
        static std::string SPOTIFY_REFRESH_TOKEN;
        static std::string hasAccessToken(std::pair<dpp::cluster&, dpp::snowflake> event, std::string& endpoint, std::string history_entry, size_t songs);
        static void makeRequest(std::pair<dpp::cluster&, dpp::snowflake> event, std::string endpoint, std::string history_entry, size_t songs = 0);
        static void handleReply(std::pair<dpp::cluster&, dpp::snowflake> event, const dpp::http_request_completion_t& reply, std::string history_entry, size_t songs);
        static void handleTrack(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& track, std::string history_entry);
        static void handleAlbum(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, std::string history_entry, size_t songs);
        static void handlePlaylist(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, std::string history_entry, size_t songs);
};

#endif