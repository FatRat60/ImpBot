#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <dpp/dpp.h>
#include <mutex>
#include <deque>

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
        music_queue() { stopLivestream = false; page = 0; }
        bool enqueue(dpp::discord_voice_client* vc, song& song_to_add);
        bool go_next(dpp::discord_voice_client* vc);
        void skip(dpp::discord_voice_client* vc);
        void clear_queue();
        bool remove_from_queue(size_t ind);
        dpp::message get_queue_embed();
        bool empty() { return queue.empty(); }
        size_t getPage() { return page; }
        dpp::message new_page(size_t num) {page = num; return get_queue_embed(); }
    private:
        std::mutex queue_mutex;
        std::deque<song> queue;
        bool stopLivestream;
        size_t page;
        bool handle_download(dpp::discord_voice_client* vc);
        bool preload();
};

#endif