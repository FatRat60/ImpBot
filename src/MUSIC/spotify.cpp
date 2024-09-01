#include "spotify.h"

std::string spotify::SPOTIFY_CLIENT_ID;
std::string spotify::SPOTIFY_CLIENT_SECRET;
std::string spotify::SPOTIFY_ACCESS_TOKEN;
std::string spotify::SPOTIFY_REFRESH_TOKEN;
std::mutex spotify::token_mutex;

void spotify::parseURL(std::pair<dpp::cluster&, dpp::snowflake> event, std::string url, std::string history_entry)
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

            makeRequest(event, std::string(SPOTIFY_ENDPOINT) + endpoint, history_entry);
        }
    }
}

/*Will check if there is an access token, returns access token if so and the thread that called
hasAccessToken will make http request. Otherwise it makes the POST call to refresh/create token and 
in that callback will make the http request. Calling thread should do nothing if returns empty str
*/
std::string spotify::hasAccessToken(std::pair<dpp::cluster&, dpp::snowflake> event, std::string& endpoint, std::string history_entry, size_t songs)
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
        event.first.request(
            url, dpp::m_post,
            [event, endpoint, history_entry, songs](const dpp::http_request_completion_t& reply){
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
                        event.first.start_timer(
                            [bot = &event.first](const dpp::timer& timer){
                                std::lock_guard<std::mutex> guard(spotify::token_mutex);
                                spotify::SPOTIFY_ACCESS_TOKEN = "";
                                bot->stop_timer(timer);
                            }, expire_time
                        );
                    }
                    
                    // with new credentials make the new request
                    event.first.request(
                        endpoint, dpp::m_get,
                        [event, history_entry, songs](const dpp::http_request_completion_t& reply){
                            spotify::handleReply(event, reply, history_entry, songs);
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

void spotify::makeRequest(std::pair<dpp::cluster&, dpp::snowflake> event, std::string endpoint, std::string history_entry, size_t songs)
{
    std::string access_token = hasAccessToken(event, endpoint, history_entry, songs);
    if (!access_token.empty())
        event.first.request(
            endpoint, dpp::m_get,
            [event, songs, history_entry](const dpp::http_request_completion_t& reply){
                spotify::handleReply(event, reply, history_entry, songs);
            }, "", "", { {"Authorization", "Bearer  " + access_token} }
        );
}

void spotify::handleReply(std::pair<dpp::cluster&, dpp::snowflake> event, const dpp::http_request_completion_t& reply, std::string history_entry, size_t songs)
{
    if (reply.status == 200)
    {
        dpp::json json = dpp::json::parse(reply.body);
        // dealing with an album or playlist
        if (json.contains("items"))
        {
            handleAlbum(event, json, history_entry, songs);
        }
        else if (json.contains("tracks"))
        {
            std::string name = json["name"].get<std::string>();
            if (name.empty())
                name = "Unknown";
            handlePlaylist(event, json["tracks"], history_entry + " queued from " + name, songs);
        }
        else
        {
            handleTrack(event, json, history_entry);
            std::string name = json["name"];
            music_queue* queue = music_queue::getQueue(event.second);
            if (queue)
                queue->addHistory(history_entry + " queued " + json["name"].get<std::string>());
        }
    }
    else if (reply.status == 429)
    {
        //event.edit_original_response(dpp::message("Rate-limited. No more spotify until tomorrow. :("));
    }
    //else
        //event.edit_original_response(dpp::message("There was an issue with queueing that song. Please try smth else"));
}

void spotify::handleTrack(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& track, std::string history_entry)
{
    // build search query for yt : "<track name> <artist names>"
    std::string query;
    query += track["name"];
    if (track["artists"].size() > 0)
    {
        std::string name = track["artists"][0]["name"];
        query += " " + name;
    }
    std::thread t([event, query](){ youtube::ytsearch(event, query); });
    t.detach();
}

void spotify::handleAlbum(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& album, std::string history_entry, size_t songs)
{
    // iterate through items
    for (dpp::json track : album["items"])
    {
        if (track.contains("name"))
        {
            songs++;
            handleTrack(event, track, "");
        }
    }

    // go to next page if it exists
    if (!album["next"].is_null())
    {
        std::string next = album["next"];
        makeRequest(event, next, history_entry, songs);
    }
    else
    {        
        music_queue* queue = music_queue::getQueue(event.second);
        if (queue)
            queue->addHistory(history_entry + " " + std::to_string(songs) + " songs (Spotify)");
    }
}

void spotify::handlePlaylist(std::pair<dpp::cluster&, dpp::snowflake> event, dpp::json& playlist, std::string history_entry, size_t songs)
{
    // iterate through items
    for (dpp::json track : playlist["items"])
    {
        if (track.contains("name") || track.contains("track"))
        {
            songs++;
            handleTrack(event, track.contains("name") ? track : track["track"], "");
        }
    }

    // go to next page if it exists
    if (!playlist["next"].is_null())
    {
        std::string next = playlist["next"];
        if (playlist["type"] == "playlist")
        {
            size_t equals = next.rfind('=');
            std::string fields = next.substr(equals+1);
            next = next.substr(0, equals+1) + dpp::utility::url_encode(fields);
        }
        makeRequest(event, next, history_entry, songs);
    }
    else
    {
        music_queue* queue = music_queue::getQueue(event.second);
        if (queue)
            queue->addHistory(history_entry + " " + std::to_string(songs) + " songs (Spotify)");
    }
}
