#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <dpp/dpp.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <random>
#include <chrono>

#define NEXT_SONG "resources/next.pcm"
#define MAX_EMBED_VALUES 10
#define MAX_HISTORY_ENTRIES 25
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
        static void removeQueue(std::pair<dpp::discord_client&, dpp::snowflake> event);
        static void cacheMessage(dpp::message& msg);
        static dpp::message* getMessage(dpp::snowflake msg_id){ return player_embed_cache.find(msg_id); }
        static void removeMessage(dpp::snowflake msg_id);
        static void updateMessage(std::pair<dpp::cluster&, dpp::snowflake> event);
        music_queue() { stopLivestream.store(false); page_number.store(0); vc = nullptr; page.store(playback_control); player_id.store(0); didShuffle = false; }
        void setPlayerID(dpp::snowflake new_id) { player_id.store(new_id); }
        dpp::snowflake getPlayerID(){ return player_id.load(); }
        void setVoiceClient(dpp::discord_voice_client* voice);
        bool enqueue(song& song_to_add);
        bool go_next();
        bool skip();
        void clear_queue();
        bool remove_from_queue(size_t start, size_t end);
        dpp::message get_embed();
        bool empty() { return queue.empty(); }
        void changePageNumber(int inc_value);
        void setPage(page_type new_page){ page.store(new_page); page_number.store(0); }
        page_type getPage(){ return page.load(); }
        void pause(){ vc->pause_audio(!vc->is_paused());}
        void addHistory(std::string new_entry);
        void shuffle();
    private:
        static std::unordered_map<dpp::snowflake, music_queue*> queue_map;
        static std::shared_mutex map_mutex;
        static dpp::cache<dpp::message> player_embed_cache;
        dpp::discord_voice_client* vc; // dpp::discord_voice_client*
        std::mutex queue_mutex;
        std::mutex history_mutex;
        std::mutex download_mutex;
        std::condition_variable vc_ready;
        std::deque<song> queue;
        std::deque<std::string> history;
        std::atomic_bool stopLivestream;
        bool didShuffle;
        std::atomic<page_type> page; // page_type
        std::atomic_size_t page_number; // size_t
        std::atomic<dpp::snowflake> player_id; // dpp::snowflake
        void handle_download(std::string url);
        dpp::message get_queue_embed();
        dpp::message get_history_embed();
        dpp::message get_playback_embed();
};

#endif