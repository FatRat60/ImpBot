#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <dpp/dpp.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <unordered_map>

#define NEXT_SONG "resources/next.pcm"
#define MAX_EMBED_VALUES 10

enum song_type {
    video,
    livestream
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
        music_queue() { stopLivestream = false; page = 0; vc = nullptr; }
        void setVoiceClient(dpp::discord_voice_client* voice);
        bool enqueue(song& song_to_add);
        bool go_next();
        void skip();
        void clear_queue();
        bool remove_from_queue(size_t start, size_t end);
        dpp::message get_queue_embed();
        bool empty() { return queue.empty(); }
        size_t getPage() { return page; }
        dpp::message new_page(size_t num) {page = num; return get_queue_embed(); }
        void shuffle();
    private:
        static std::unordered_map<dpp::snowflake, music_queue*> queue_map;
        static std::mutex map_mutex;
        dpp::discord_voice_client* vc;
        std::mutex queue_mutex;
        std::condition_variable vc_ready;
        std::deque<song> queue;
        bool stopLivestream;
        size_t page;
        bool handle_download(std::string url);
        bool preload(std::string url);
};

#endif