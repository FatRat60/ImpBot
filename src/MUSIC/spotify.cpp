#include "spotify.h"

std::string spotify::SPOTIFY_CLIENT_ID;
std::string spotify::SPOTIFY_CLIENT_SECRET;
std::string spotify::SPOTIFY_ACCESS_TOKEN;
std::string spotify::SPOTIFY_REFRESH_TOKEN;
std::mutex spotify::token_mutex;

void spotify::parseURL(song_event& event, std::string url)
{
    // remove / from beginning
    url.erase(0, 1);
    // parse
    size_t slash = url.find('/');
    if (slash != std::string::npos)
    {
        // get id
        size_t mark = url.find('?', slash);
        if (slash+1 < url.size())
        {
            std::string id = url.substr(slash+1, (mark-slash)-1);
            // get type of song
            std::string type = url.substr(0, slash);
            std::string endpoint;
            if (type == "playlist")
                endpoint = "/playlists/" + id + "?market=US&fields=" + dpp::utility::url_encode("name,type,tracks(next,items.track(artists.name,name))");
            else if (type == "album")
                endpoint = "/albums/" + id + "?market=US";
            else
                endpoint = "/tracks/" + id + "?market=US";

            makeRequest(event, std::string(SPOTIFY_ENDPOINT) + endpoint);
        }
    }
}

/*Will check if there is an access token, returns access token if so and the thread that called
hasAccessToken will make http request. Otherwise it makes the POST call to refresh/create token and 
in that callback will make the http request. Calling thread should do nothing if returns empty str
*/
std::string spotify::hasAccessToken(song_event& event, std::string& endpoint, size_t songs)
{
    std::lock_guard<std::mutex> guard(token_mutex);
    // no access token
    if (SPOTIFY_ACCESS_TOKEN.empty())
    {
        std::string url = "https://accounts.spotify.com/api/token?";
        // Generating token for first time
        if (SPOTIFY_REFRESH_TOKEN.empty())
        {
            if (!SPOTIFY_CLIENT_ID.empty() && !SPOTIFY_CLIENT_SECRET.empty())
                url  += "grant_type=client_credentials&client_id=" + SPOTIFY_CLIENT_ID + "&client_secret=" + SPOTIFY_CLIENT_SECRET;
            else
            {
                //event.edit_original_response(dpp::message("Need client id and secret to generete access token.\nQueuing from spotify is disabled for now"));
            }
        }
        else
            url += "grant_type=refresh_token&refresh_token=" + SPOTIFY_REFRESH_TOKEN;
        // call api
        event.bot.request(
            url, dpp::m_post,
            [event, endpoint, songs](const dpp::http_request_completion_t& reply) mutable {
                if (reply.status == 200)
                {
                    std::lock_guard<std::mutex> guard(spotify::token_mutex);
                    dpp::json json = dpp::json::parse(reply.body);
                    spotify::SPOTIFY_ACCESS_TOKEN = json["access_token"];
                    // check if access token was provided
                    if (json.contains("refresh_token"))
                        spotify::SPOTIFY_REFRESH_TOKEN = json["refresh_token"];
                    
                    // set timer for token expiration
                    if (json.contains("expires_in"))
                    {
                        size_t expire_time = json["expires_in"];
                        if (expire_time > 30)
                            expire_time -= 30;
                        event.bot.start_timer(
                            [&bot = event.bot](const dpp::timer& timer){
                                std::lock_guard<std::mutex> guard(spotify::token_mutex);
                                spotify::SPOTIFY_ACCESS_TOKEN = "";
                                bot.stop_timer(timer);
                            }, expire_time
                        );
                    }
                    
                    // with new credentials make the new request
                    event.bot.request(
                        endpoint, dpp::m_get,
                        [event, songs](const dpp::http_request_completion_t& reply) mutable {
                            spotify::handleReply(event, reply, songs);
                        }, "", "", { {"Authorization", "Bearer  " + spotify::SPOTIFY_ACCESS_TOKEN} }
                    );
                }
                //else
                    //event.edit_original_response(dpp::message("there was an error getting authorization:\n" + reply.body));
            }, "", "application/x-www-form-urlencoded"
        );
        return "";
    }
    else
        return SPOTIFY_ACCESS_TOKEN;
}

void spotify::makeRequest(song_event& event, std::string endpoint, size_t songs)
{
    std::string access_token = hasAccessToken(event, endpoint, songs);
    if (!access_token.empty())
        event.bot.request(
            endpoint, dpp::m_get,
            [event, songs](const dpp::http_request_completion_t& reply) mutable {
                spotify::handleReply(event, reply, songs);
            }, "", "", { {"Authorization", "Bearer  " + access_token} }
        );
}

void spotify::handleReply(song_event& event, const dpp::http_request_completion_t& reply, size_t songs)
{
    if (reply.status == 200)
    {
        dpp::json json = dpp::json::parse(reply.body);
        // dealing with an album or playlist
        if (json.contains("items"))
        {
            handlePlaylistItems(event, json, songs);
        }
        else if (json.contains("tracks"))
        {
            std::string name = json["name"].get<std::string>();
            if (name.empty())
                name = "Unknown";
            handlePlaylist(event.appendHistory(" queued from " + name), json, songs);
        }
        else
        {
            music_queue* queue = music_queue::getQueue(event.guild_id);
            if (queue)
                queue->addHistory(event.history_entry + " queued " + json["name"].get<std::string>());
            handleTrack(event.zeroHistory(), json);
        }
    }
    else if (reply.status == 429)
    {
        //event.edit_original_response(dpp::message("Rate-limited. No more spotify until tomorrow. :("));
    }
    //else
        //event.edit_original_response(dpp::message("There was an issue with queueing that song. Please try smth else"));
}

void spotify::handleTrack(song_event& event, dpp::json& track)
{
    // build search query for yt : "<track name> <artist names>"
    std::string query;
    query += track["name"];
    if (track["artists"].size() > 0)
    {
        std::string name = track["artists"][0]["name"];
        query += " " + name;
    }
    std::thread t([event, query]() mutable { youtube::ytsearch(event, query); });
    t.detach();
}

void spotify::handlePlaylistItems(song_event& event, dpp::json& playlistItem, size_t songs)
{
    song_event event_zerod = {
        event.bot, event.guild_id,
        event.shuffle, ""
    };

    if (event.shuffle)
    {
        auto rng_seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(playlistItem["items"].begin(), playlistItem["items"].end(), std::default_random_engine(rng_seed));
    }
    // iterate through items
    for (dpp::json track : playlistItem["items"])
    {
        if (track.contains("name") || track.contains("track"))
        {
            songs++;
            handleTrack(event_zerod, track.contains("name") ? track : track["track"]);
        }
    }

    // go to next page if it exists
    if (!playlistItem["next"].is_null())
    {
        std::string next = playlistItem["next"].get<std::string>();
        if (!playlistItem.contains("href"))
        {
            size_t equals = next.rfind('=');
            next = next.substr(0, equals+1) + dpp::utility::url_encode("next,items(track(name,artists.name))");
        }
        makeRequest(event, next, songs);
    }
    else
    {        
        music_queue* queue = music_queue::getQueue(event.guild_id);
        if (queue)
            queue->addHistory(event.history_entry + " " + std::to_string(songs) + " songs (Spotify)");
    }
}

void spotify::handlePlaylist(song_event& event, dpp::json& playlist, size_t songs)
{
    song_event event_zerod = {
        event.bot, event.guild_id,
        event.shuffle, ""
    };

    if (event.shuffle)
    {
        auto rng_seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(playlist["tracks"]["items"].begin(), playlist["tracks"]["items"].end(), std::default_random_engine(rng_seed));
    }
    // iterate through items
    for (dpp::json track : playlist["tracks"]["items"])
    {
        if (track.contains("name") || track.contains("track"))
        {
            songs++;
            handleTrack(event_zerod, track.contains("name") ? track : track["track"]);
        }
    }

    // go to next page if it exists
    if (!playlist["tracks"]["next"].is_null())
    {
        std::string next = playlist["tracks"]["next"].get<std::string>();
        if (playlist["type"].get<std::string>() == "playlist")
        {
            size_t equals = next.rfind('=');
            next = next.substr(0, equals+1) + dpp::utility::url_encode("next,items(track(name,artists.name))");
        }
        makeRequest(event, next, songs);
    }
    else
    {
        music_queue* queue = music_queue::getQueue(event.guild_id);
        if (queue)
            queue->addHistory(event.history_entry + " " + std::to_string(songs) + " songs (Spotify)");
    }
}
