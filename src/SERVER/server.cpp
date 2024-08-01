#include "server.h"
#include <fstream>
#include <sys/stat.h>
#include <array>

std::string server::server_dir;
std::string server::active_container;
std::vector<std::string> server::get_available_servers()
{
    std::vector<std::string> servers;
    std::ifstream file(server_dir + std::string("/") + std::string(SERVER_LIST_FILE));

    for (std::string line; std::getline(file, line); )
    {
        auto end = std::end(line);
        if (*(--end) == '\n')
            line.pop_back(); // bye bye \n
        servers.push_back(line); // add to vector
    }

    file.close();

    return servers;
}

void server::start(const dpp::slashcommand_t& event)
{   
    if (active_container.empty())
    {
        if (dirExists())
        {
            std::string server_name = std::get<std::string>(event.get_parameter("name"));
            if (!server_name.empty())
            {
                std::string path = server_dir + "/" + server_name;
                struct stat sb;
                // invalid server name provided
                if (stat(path.c_str(), &sb) != 0)
                {
                    event.reply("No server installed with name: " + server_name);
                    return;
                }

                event.reply("Starting " + server_name + " server. Please wait...");
                std::string cmd = "docker compose -f " + path + "/docker-compose.yml up -d --wait";
                int res = system(cmd.c_str());
                if (res == 0)
                {
                    // TODO print more useful info like ip and stuff                    
                    event.edit_original_response(dpp::message(server_name + " server started successfuly!"));
                    active_container = server_name;
                }
                else
                    event.edit_original_response(dpp::message("Could not start server"));
            }
            else
                event.reply("Please provide name for server you want to start");
        }
        else
            event.reply("Server base directory was not set properly. Cannot start server");
    }
    else
    {
        event.reply("There is a " + active_container + " server running already");
    }
}

void server::terminate(const dpp::slashcommand_t& event)
{
    if (!active_container.empty())
    {
        std::string path = server_dir + "/" + active_container + "/docker-compose.yml";
        std::string cmd = "docker compose -f " + path + " stop";
        event.reply("Shutting down " + active_container + " server. Please wait...");
        int res = system(cmd.c_str());
        if (res == 0)
        {
            bool restart;
            try { restart = std::get<bool>(event.get_parameter("restart")); }
            catch(const std::exception& e){ restart = false; }
             
            if (restart)
            {
                event.edit_original_response(dpp::message("Restarting " + active_container + " server. Please wait..."));
                cmd = "docker compose -f " + path + " up -d --wait";
                res = system(cmd.c_str());
                if (res == 0)
                {
                    event.edit_original_response(dpp::message(active_container + " server started successfuly!"));
                }
                else
                {
                    event.edit_original_response(dpp::message("There was an issue restarting the server"));
                    active_container = "";
                }
            }
            else
            {
                event.edit_original_response(dpp::message(active_container + " server stopped successfully"));
                active_container = "";
            }
        }
    }
    else
        event.reply("No server currently running. Start one with /start <name>");
}