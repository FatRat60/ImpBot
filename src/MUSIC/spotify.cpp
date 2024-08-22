#include "spotify.h"

std::string spotify::SPOTIFY_CLIENT_ID;
std::string spotify::SPOTIFY_CLIENT_SECRET;
std::string spotify::SPOTIFY_ACCESS_TOKEN;
std::string spotify::SPOTIFY_REFRESH_TOKEN;
std::mutex spotify::token_mutex;

void spotify::parseURL(const dpp::slashcommand_t& event, std::string url)
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
                endpoint = "/playlists/" + id + "/tracks";
            else if (type == "album")
                endpoint = "/albums/" + id + "/tracks";
            else
                endpoint = "/tracks/" + id;

            std::string access_token = hasAccessToken(event, endpoint);
            if (!access_token.empty())
            {
                //makeRequest(endpoint);
                return;
            }
        }
    }
    event.edit_original_response(dpp::message("Could not parse spotify link."));
}

/*Will check if there is an access token, returns access token if so and the thread that called
hasAccessToken will make http request. Otherwise it makes the POST call to refresh/create token and 
in that callback will make the http request. Calling thread should do nothing if returns empty str
*/
std::string spotify::hasAccessToken(const dpp::slashcommand_t& event, std::string& endpoint)
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
            [event, endpoint](const dpp::http_request_completion_t& reply){
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
                        if (expire_time > 10)
                            expire_time -= 10;
                        event.from->creator->start_timer(
                            [bot = event.from->creator](const dpp::timer& timer){
                                std::lock_guard<std::mutex> guard(spotify::token_mutex);
                                spotify::SPOTIFY_ACCESS_TOKEN = "";
                                bot->stop_timer(timer);
                            }, expire_time
                        );
                    }
                    
                    // with new credentials make the new request
                    // spotify::makeRequest(endpoint);
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
