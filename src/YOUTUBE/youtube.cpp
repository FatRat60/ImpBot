#include "youtube.h"
#include <dpp/dpp.h>
#include <string>
#include <iostream>

std::string youtube::YOUTUBE_API_KEY;

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

void youtube::post_search(dpp::http_request_completion_t result, dpp::cluster& bot)
{
    std::cout << result.body << "\n";
}