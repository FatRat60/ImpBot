#include "youtube.h"
#include <iostream>

// yt-dlp -x --audio-format opus -o test.opus -S +size --no-playlist --force-overwrites --fixup never --extractor-args youtube:player_client=web URL

std::string youtube::YOUTUBE_API_KEY;
bool youtube::downloading = false;

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

std::string youtube::post_search(dpp::http_request_completion_t result)
{
    size_t ind = result.body.find('[');
    std::string found_url;
    if (result.status == 200 && ind != std::string::npos)
    {
        char char_path[] = {'{', '{', ':', ':'};
        for (int i = 0; i < 4; i++){
            ind = result.body.find(char_path[i], ind + 1);
            if (ind == std::string::npos)
                break;
        }

        if (ind != std::string::npos)
        {
            size_t end = result.body.find('\n', ind);
            ind += 3;
            found_url = YOUTUBE_URL + result.body.substr(ind, end - ind - 1);
        }
    }
    return found_url;
}

/*Used by discord bot in /play to download songs*/
bool youtube::download(std::string& url)
{
    downloading = true;
    std::string command = "\"yt-dlp\" \"-x\" \"--audio-format\" \"opus\" \"-o\" \"temp/song.opus\" \"-S\" \"+size\" \"--no-playlist\" \"--force-overwrites\" \"--fixup\" \"never\" \"--extractor-args\" \"youtube:player_client=web\" \"" + url + "\"";
    int result = system(command.c_str());
    downloading = false;
    return result == -1;
}