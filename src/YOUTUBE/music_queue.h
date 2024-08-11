#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <dpp/dpp.h>
#include <mutex>
#include <deque>

#define NEXT_SONG "resources/next.pcm"

enum song_type {
    video,
    playlist,
    livestream
};

struct song
{
    song_type type;
    std::string url;
    std::string title;
    std::string thumbnail;
};

class music_queue
{
    public:
        music_queue() { stopLivestream = false; };
        bool enqueue(dpp::discord_voice_client* vc, song& song_to_add);
        bool go_next(dpp::discord_voice_client* vc);
        void skip(dpp::discord_voice_client* vc);

    private:
        std::mutex queue_mutex;
        std::deque<song> queue;
        bool stopLivestream;
        bool handle_download(dpp::discord_voice_client* vc);
        bool preload();
};

#endif