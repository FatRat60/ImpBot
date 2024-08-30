#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <dpp/dpp.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <unordered_map>

#define NEXT_SONG "resources/next.pcm"
#define MAX_EMBED_VALUES 10
#define DEFAULT_THUMBNAIL "https://e7.pngegg.com/pngimages/434/614/png-clipart-question-mark-graphy-big-question-mark-text-trademark.png"

enum song_type {
    video,
    livestream
};

enum page_type {
    playback_control,
    queue,
    history
};

struct song
{
    song_type type;
    std::string url;
    std::string title;
    std::string duration;
    std::string thumbnail;
};



class music_queue
{
    public:
        static music_queue* getQueue(dpp::snowflake guild_id, bool create = false);
        static void removeQueue(dpp::snowflake guild_id);
        static void cacheMessage(dpp::message& msg);
        static dpp::message* getMessage(dpp::snowflake msg_id){ return player_embed_cache.find(msg_id); }
        static void removeMessage(dpp::snowflake msg_id);
        music_queue() { stopLivestream = false; page = 0; vc = nullptr; curr_page = playback_control; player_id = 0; }
        void setPlayerID(dpp::snowflake new_id) { player_id = new_id; }
        dpp::snowflake getPlayerID(){ return player_id; }
        void setVoiceClient(dpp::discord_voice_client* voice);
        bool enqueue(song& song_to_add);
        bool go_next();
        void skip();
        void clear_queue();
        bool remove_from_queue(size_t start, size_t end);
        dpp::message get_embed();
        bool empty() { return queue.empty(); }
        size_t getPage() { return page; }
        dpp::message new_page(size_t num) {page = num; return get_queue_embed(); }
        void shuffle();
    private:
        static std::unordered_map<dpp::snowflake, music_queue*> queue_map;
        static std::mutex map_mutex;
        static dpp::cache<dpp::message> player_embed_cache;
        dpp::discord_voice_client* vc;
        std::mutex queue_mutex;
        std::condition_variable vc_ready;
        std::deque<song> queue;
        bool stopLivestream;
        page_type curr_page;
        size_t page;
        dpp::snowflake player_id;
        bool handle_download(std::string url);
        bool preload(std::string url);
        dpp::message get_queue_embed();
        dpp::message get_history_embed();
        dpp::message get_playback_embed();
};

#endif