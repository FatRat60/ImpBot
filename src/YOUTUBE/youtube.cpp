#include "youtube.h"
#include <iostream>
#include <cstdlib>

// yt-dlp -x --audio-format opus -o test.opus -S +size --no-playlist --force-overwrites --fixup never --extractor-args youtube:player_client=web URL

std::string youtube::YOUTUBE_API_KEY;

bool youtube::isLink(std::string& query)
{
    return query.substr(0, 8).compare("https://") == 0;
}

std::string youtube::pipe_replace(std::string& query)
{
    std::string q;
    size_t cur_ind = 0;
    size_t nex_ind;
    while ( (nex_ind = query.find(' ', cur_ind)) != std::string::npos)
    {
        q += query.substr(cur_ind, nex_ind - cur_ind) + "%2C";
        cur_ind = nex_ind + 1; // start at char to right of space we just found
    }
    q += query.substr(cur_ind, query.length() - cur_ind);
    return q;
}

void youtube::post_search(dpp::http_request_completion_t result, song& youtube_song)
{
    dpp::json json = dpp::json::parse(result.body);
    // You really need to implement JSON here, lazy ass
    if (!json["items"].empty())
    {
        youtube_song.url = YOUTUBE_URL;
        youtube_song.url += json["items"][0]["id"]["videoId"];
        youtube_song.title = json["items"][0]["snippet"]["title"];
    }
}

/*Used by discord bot in /play to download songs*/
void youtube::download(std::string url)
{
    std::string command = "\"yt-dlp\" \"-x\" \"--audio-format\" \"opus\" \"-o\" \"temp/song.opus\" \"-S\" \"+size\" \"--no-playlist\" \"--force-overwrites\" \"--fixup\" \"never\" \"--extractor-args\" \"youtube:player_client=web\" \"" + url + "\"";
    system(command.c_str());
}