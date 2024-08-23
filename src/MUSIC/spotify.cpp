#include "spotify.h"

std::string spotify::SPOTIFY_CLIENT_ID;
std::string spotify::SPOTIFY_CLIENT_SECRET;
std::string spotify::SPOTIFY_ACCESS_TOKEN;
std::string spotify::SPOTIFY_REFRESH_TOKEN;
std::mutex spotify::token_mutex;

void spotify::parseURL(const dpp::slashcommand_t& event, music_queue* queue, std::string url)
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
                endpoint = "/playlists/" + id + "/tracks?market=US&limit=50&fields=" + dpp::utility::url_encode("next,items(track(name,artists.name))") + "&offset=0";
            else if (type == "album")
                endpoint = "/albums/" + id + "/tracks?market=US&limit=50&offset=0";
            else
                endpoint = "/tracks/" + id + "?market=US";

            makeRequest(event, queue, std::string(SPOTIFY_ENDPOINT) + endpoint);
            return;
        }
    }
    event.edit_original_response(dpp::message("Could not parse spotify link."));
}

/*Will check if there is an access token, returns access token if so and the thread that called
hasAccessToken will make http request. Otherwise it makes the POST call to refresh/create token and 
in that callback will make the http request. Calling thread should do nothing if returns empty str
*/
std::string spotify::hasAccessToken(const dpp::slashcommand_t& event, music_queue* queue, std::string& endpoint, size_t songs)
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
                event.edit_original_response(dpp::message("Need client id and secret to generete access token.\nQueuing from spotify is disabled for now"));
            }
        }
        else
            url += "grant_type=refresh_token&refresh_token=" + SPOTIFY_REFRESH_TOKEN;
        // call api
        event.from->creator->request(
            url, dpp::m_post,
            [event, endpoint, queue, songs](const dpp::http_request_completion_t& reply){
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
                        event.from->creator->start_timer(
                            [bot = event.from->creator](const dpp::timer& timer){
                                std::lock_guard<std::mutex> guard(spotify::token_mutex);
                                spotify::SPOTIFY_ACCESS_TOKEN = "";
                                bot->stop_timer(timer);
                            }, expire_time
                        );
                    }
                    
                    // with new credentials make the new request
                    event.from->creator->request(
                        endpoint, dpp::m_get,
                        [event, queue, songs](const dpp::http_request_completion_t& reply){
                            spotify::handle_reply(event, queue, reply, songs);
                        }, "", "", { {"Authorization", "Bearer  " + spotify::SPOTIFY_ACCESS_TOKEN} }
                    );
                }
                else
                    event.edit_original_response(dpp::message("there was an error getting authorization:\n" + reply.body));
            }, "", "application/x-www-form-urlencoded"
        );
        return "";
    }
    else
        return SPOTIFY_ACCESS_TOKEN;
}

void spotify::makeRequest(const dpp::slashcommand_t& event, music_queue* queue, std::string endpoint, size_t songs)
{
    std::string access_token = hasAccessToken(event, queue, endpoint, songs);
    if (!access_token.empty())
        event.from->creator->request(
            endpoint, dpp::m_get,
            [event, queue, songs](const dpp::http_request_completion_t& reply){
                spotify::handle_reply(event, queue, reply, songs);
            }, "", "", { {"Authorization", "Bearer  " + access_token} }
        );
}

void spotify::handle_reply(const dpp::slashcommand_t &event, music_queue *queue, const dpp::http_request_completion_t &reply, size_t songs)
{
    if (reply.status == 200)
    {
        dpp::json json = dpp::json::parse(reply.body);
        // dealing with an album or playlist
        if (json.contains("items"))
        {
            handle_playlist(event, queue, json, songs);
        }
        else
        {
            handle_track(event, queue, json);
            std::string name = json["name"];
            event.edit_original_response(dpp::message("Added " + name + " from Spotify"));
        }
    }
    else if (reply.status == 429)
    {
        event.edit_original_response(dpp::message("Rate-limited. No more spotify until tomorrow. :("));
    }
    else
        event.edit_original_response(dpp::message("There was an issue with queueing that song. Please try smth else"));
}

void spotify::handle_track(const dpp::slashcommand_t &event, music_queue *queue, dpp::json& track)
{
    // build search query for yt : "<track name> <artist names>"
    std::string query;
    query += track["name"];
    if (track["artists"].size() > 0)
    {
        std::string name = track["artists"][0]["name"];
        query += " " + name;
    }
    std::thread t([event, query, queue](){ youtube::ytsearch(event, query, queue, false); });
    t.detach();
}

void spotify::handle_playlist(const dpp::slashcommand_t &event, music_queue *queue, dpp::json& playlist, size_t songs)
{
    // iterate through items
    for (dpp::json track : playlist["items"])
    {
        if (track.contains("name") || track.contains("track"))
        {
            songs++;
            handle_track(event, queue, track.contains("name") ? track : track["track"]);
        }
    }

    // go to next page if it exists
    if (!playlist["next"].is_null())
    {
        std::string next = playlist["next"];
        size_t equals = next.rfind('=');
        std::string fields = next.substr(equals+1);
        makeRequest(event, queue, next.substr(0, equals+1) + dpp::utility::url_encode(fields), songs);
    }
    else
        event.edit_original_response(dpp::message("Added " + std::to_string(songs) + " songs from Spotify!"));
}
