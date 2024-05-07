#include "youtube.h"
#include <dpp/dpp.h>
#include <string>
#include <iostream>

std::string youtube::YOUTUBE_API_KEY;

std::string youtube::pipe_replace(std::string query)
{
    std::string q;
    size_t cur_ind = 0;
    size_t nex_ind;
    while ( (nex_ind = query.find(' ')) != std::string::npos)
    {
        q += query.substr(cur_ind, nex_ind - cur_ind) + "%7C";
        cur_ind = nex_ind + 1; // start at char to right of space we just found
    }
    // append last term
    q += query.substr(cur_ind, query.length() - cur_ind);
    std::cout << q << "\n\n";
    return q;
}

bool youtube::search_video(std::string query, dpp::cluster *bot)
{
    // construct req obj
    dpp::http_request req(std::string(YOUTUBE_ENDPOINT),
        nullptr,
        dpp::m_get);
    
    // init headers
    req.req_headers.insert(std::pair{"key", YOUTUBE_API_KEY}); // REQUIRED
    req.req_headers.insert(std::pair{"part", "snippet"}); // REQUIRED
    
    // search terms or URL
    std::string q = pipe_replace(query);
    req.req_headers.insert(std::pair{"q", q});

    // type 
    req.req_headers.insert(std::pair{"type", "video,playlist"});

    std::cout << "running\n";
    dpp::http_request_completion_t complete = req.run(bot);

    std::cout << complete.body << "\n";

    return complete.status == 200;
}