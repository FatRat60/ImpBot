#include "music_queue.h"

/*If first song: Download and send song to discord, then add to queue
If second song, will download locally to resources/next.pcm, then add to queue
Else, will just add to queue. No download is performed yet.
Returns true if download is succesful AND song is added to queue, else false*/
bool music_queue::enqueue(dpp::discord_voice_client* vc, song& song_to_add)
{
    // when the first song is a livestream 
    std::unique_lock<std::mutex> guard(queue_mutex); // acquire mutex for queue

    // add song to queue
    queue.push_back(song_to_add);
    // stream song or pre-download
    if (queue.size() == 1)
    {
        // stream song straight to disc
        stopLivestream = false;
        guard.unlock();
        return handle_download(vc);
    }
    else if (queue.size() == 2)
    {
        // preload
        return preload();
    }
    return true;
}

/*Will be called when streamed song is finished. Pops top of queue
then begins streaming next song which should be located on disk at resources/next.pcm
After streaming to discord, download next song if exists to resources/next.pcm
If no track is next, return false, else true*/
bool music_queue::go_next(dpp::discord_voice_client* vc)
{
    std::unique_lock<std::mutex> guard(queue_mutex);

    queue.pop_front();
    size_t size = queue.size();
    if (size > 0)
    {
        // send next song
        song next = queue.front();
        if (next.type == livestream)
        {
            stopLivestream = false;
            std::thread t([vc, this](){ this->handle_download(vc); });
            t.detach();
        }
        else
        {
            // read pcm bytes from file
            FILE* next_pcm = fopen(NEXT_SONG, "rb");
            if (next_pcm)
            {
                constexpr size_t buffsize = dpp::send_audio_raw_max_length;
                char buffer[dpp::send_audio_raw_max_length];
                size_t curr_read = 0;

                while (!vc->terminating && (curr_read = fread(buffer, sizeof(char), buffsize, next_pcm)) == buffsize)
                    vc->send_audio_raw((uint16_t*)buffer, buffsize);

                if (!vc->terminating && curr_read > 0)
                {
                    int rem = curr_read % 4;
                    vc->send_audio_raw((uint16_t*)buffer, curr_read-rem);
                }
            }
            vc->insert_marker("end");
        }

        // preload if available
        preload();
        return true;
    }
    else
        return false;
}

void music_queue::skip(dpp::discord_voice_client* vc)
{
    if (!queue.empty() && queue.front().type == livestream)
    {
        // need to signal flag to end livestream. Afterwhich a marker will get added and trigger
        std::lock_guard<std::mutex> guard(queue_mutex);
        stopLivestream = true;
        vc->skip_to_next_marker();
    }
    else // jus skipping a song
    {
        vc->skip_to_next_marker();
        go_next(vc);
    }
}

/*This function handles streaming of song to discord. Will release mutex and block if stream is live
to allow queue to still add songs. Else holds mutex until download is finished*/
bool music_queue::handle_download(dpp::discord_voice_client* vc)
{
    song song = queue.front();
    std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + song.url + " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet pipe:.pcm";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        return false;
    constexpr size_t buffsize = dpp::send_audio_raw_max_length;
    char buffer[dpp::send_audio_raw_max_length];
    size_t curr_read = 0;

    while (!vc->terminating && !stopLivestream && (curr_read = fread(buffer, sizeof(char), buffsize, pipe.get())) == buffsize)
        vc->send_audio_raw((uint16_t*)buffer, buffsize);

    if (!vc->terminating && !stopLivestream && curr_read > 0)
    {
        int rem = curr_read % 4;
        vc->send_audio_raw((uint16_t*)buffer, curr_read-rem);
    }
    vc->insert_marker("end");

    return true;
}

bool music_queue::preload()
{
    if (queue.size() > 1)
    {
        song song = queue.at(1);
        if (song.type != livestream)
        {
            std::string cmd = "yt-dlp -f 140/139/234/233 -q --no-warnings -o - --no-playlist " + song.url + 
            " | ffmpeg -i pipe:.m4a -f s16le -ar 48000 -ac 2 -loglevel quiet -y " + std::string(NEXT_SONG);
            return system(cmd.c_str()) == 0;
        }
        return true;
    }
    else
        return false;
}
