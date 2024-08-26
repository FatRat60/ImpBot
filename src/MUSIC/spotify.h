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
        static void parseURL(const dpp::slashcommand_t& event, music_queue* queue, std::string url);
    private:
        static std::mutex token_mutex;
        static std::string SPOTIFY_CLIENT_ID;
        static std::string SPOTIFY_CLIENT_SECRET;
        static std::string SPOTIFY_ACCESS_TOKEN;
        static std::string SPOTIFY_REFRESH_TOKEN;
        static std::string hasAccessToken(const dpp::slashcommand_t& event, music_queue* queue, std::string& endpoint, size_t songs);
        static void makeRequest(const dpp::slashcommand_t& event, music_queue* queue, std::string endpoint, size_t songs = 0);
        static void handleReply(const dpp::slashcommand_t& event, music_queue* queue, const dpp::http_request_completion_t& reply, size_t songs);
        static void handleTrack(const dpp::slashcommand_t& event, music_queue* queue, dpp::json& track);
        static void handlePlaylist(const dpp::slashcommand_t& event, music_queue* queue, dpp::json& playlist, size_t songs);
};

#endif