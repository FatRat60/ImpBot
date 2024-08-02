#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <string>
#include <dpp/dpp.h>
#include <thread>

#define YOUTUBE_ENDPOINT "https://www.googleapis.com/youtube/v3/search"
#define YOUTUBE_URL "https://www.youtube.com/watch?v="
#define TRACK_FILE "temp/song.opus"
#define YUI "resources/yaharo.opus"
#define MAX_EMBED_VALUES 10

enum song_type {
    video,
    playlist,
    livestrean
};

struct song
{
    std::string url;
    std::string title;
    std::string duration;
};

class youtube
{
    public:
        static void play(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void pause(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void stop(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void skip(dpp::cluster& bot, const dpp::slashcommand_t& event);
        static void queue(const dpp::slashcommand_t& event);
        static void remove(const dpp::slashcommand_t& event);
        static void setAPIkey(std::string API_KEY) { YOUTUBE_API_KEY = API_KEY;}
    private:
        static std::string YOUTUBE_API_KEY;
        static bool isLink(std::string& query); // returns true if query begins with "https://"
        static std::string pipe_replace(std::string& query);
        static void post_search(const dpp::slashcommand_t& event, dpp::json& body);
        static void download(std::string url);
        static void send_music_buff(dpp::discord_voice_client *voice_client, std::string& song_data, bool add_start_marker);
        static dpp::embed create_list_embed(std::string title, std::string footer, std::string contents[10], int num_comp);
};  

#endif