#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "music_queue.h"
#include <string>
#include <dpp/dpp.h>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3"
#define YOUTUBE_VIDEO_URL "https://www.youtube.com/watch?v="
#define YOUTUBE_LIST_URL "https://www.youtube.com/playlist?list="
#define YUI "resources/yaharo.opus"

class youtube
{
    public:
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY;}
        static void handle_video(const dpp::slashcommand_t& event, std::string videoId, music_queue* queue, bool doReply = true);
        static void handle_playlist(const dpp::slashcommand_t& event, std::string playlistId, music_queue* queue);
    private:
        static std::string YOUTUBE_API_KEY;
        static song get_song_info(std::string& query);
        static song create_song(std::string data);
        static void handle_playlist_item(const dpp::slashcommand_t& event, dpp::json& playlistItem, music_queue* queue, size_t songs = 0);
        static void handle_video_dlp(const dpp::slashcommand_t& event, std::string videoId, music_queue* queue, bool doReply = true);
        static void handle_playlist_dlp(const dpp::slashcommand_t& event, std::string playlistId, music_queue* queue);
        static std::string convertDuration(std::string old_duration);
};  

#endif