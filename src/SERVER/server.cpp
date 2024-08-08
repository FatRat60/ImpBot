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

void server::checkForActiveServer(dpp::cluster& bot)
{
    auto potential_servers = get_available_servers();
    for (std::string server_name : potential_servers)
    {
        std::array<char, 32> buffer;
        std::string line;
        std::string cmd = "docker ps -q -f \"name=" + server_name + "\"";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
        {
            line += buffer.data();
        }
        if (!line.empty())
        {
            active_container = server_name;
            bot.log(dpp::loglevel::ll_info, "Found " + server_name + " server!");
        }
    }
}

std::string server::getIP()
{
    std::string ip = "???.???.???.???";
    std::array<char, 50> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("hostname -I", "r"), pclose);
    if (pipe)
    {
        std::string res;
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
            res += buffer.data();
        size_t ind = res.find(' ');
        if (ind != std::string::npos)
            ip = res.substr(0, ind);
        else
        {
            res.pop_back();
            ip = res;
        }
    }
    return ip;
}

dpp::embed server::createServerEmbed()
{
    dpp::embed server_embed = dpp::embed()
        .set_color(dpp::colors::pink_rose)
        .set_title("Your server is ready!")
        .set_thumbnail(MIKU_THUMBNAIL)
        .set_footer(
            dpp::embed_footer()
            .set_text("Jesus loves you!")
        )
        .set_timestamp(time(0));

    YAML::Node config = YAML::LoadFile(server_dir + "/" + active_container + "/docker-compose.yml");
    YAML::Node service = config["services"].begin()->second;

    // minecraft server
    if (active_container == "minecraft")
    {
        YAML::Node env = service["environment"];
        if (env)
        {
            server_embed.add_field("Game", "Minecraft");
            if (env["CF_SLUG"])
                server_embed.add_field("Mod Pack", env["CF_SLUG"].as<std::string>());
            else
                server_embed.add_field("Mod Pack", "Vanilla");
            if (env["VERSION"])
                server_embed.add_field("Game Version", env["VERSION"].as<std::string>());
        }
    }

    return server_embed;
}

std::string server::getPort()
{
    std::string port = "????";
    if (!active_container.empty())
    {
        std::string path = server_dir + "/" + active_container + "/docker-compose.yml";
        YAML::Node config = YAML::LoadFile(path);
        YAML::Node service = config["services"].begin()->second;
        if (service["ports"])
        {
            std::string line = service["ports"][0].as<std::string>();
            size_t ind = line.find(':');
            if (ind != std::string::npos)
                port = line.substr(ind+1);
        }
    }
    return port;
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

                event.reply("Starting " + server_name + " server. Please wait...",
                [event, server_name, path](const dpp::confirmation_callback_t& callback){
                    if (!callback.is_error())
                    {
                        std::string cmd = "docker compose -f " + path + "/docker-compose.yml up -d --wait";
                        int res = system(cmd.c_str());
                        if (res == 0)
                        {
                            // TODO print more useful info like ip and stuff                
                            active_container = server_name;    
                            event.edit_original_response(dpp::message(createServerEmbed()));
                        }
                        else
                        {
                            dpp::message msg = dpp::message("Could not start server. Refer to the attached log file");
                            cmd = "docker compose -f " + path + "/docker-compose.yml stop";
                            event.edit_original_response(dpp::message("Could not start server"));
                            system(cmd.c_str());
                            cmd = "docker compose -f " + path + "/docker-compose.yml logs >> log.txt";
                            system(cmd.c_str());
                            msg.add_file("log.txt", dpp::utility::read_file(path + std::string("/log.txt")));
                            event.edit_original_response(msg);
                        }
                    }
                });
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
        event.reply("Shutting down " + active_container + " server. Please wait...",
        [event, path](const dpp::confirmation_callback_t& callback){
            std::string cmd = "docker compose -f " + path + " stop";
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
                        event.edit_original_response(dpp::message(createServerEmbed()));
                    }
                    else
                    {
                        cmd = "docker compose -f " + path + "/docker-compose.yml stop";
                        event.edit_original_response(dpp::message("There was an issue restarting the server"));
                        system(cmd.c_str());
                    }
                }
                else
                {
                    event.edit_original_response(dpp::message(active_container + " server stopped successfully"));
                    active_container = "";
                }
            }
        });
    }
    else
        event.reply("No server currently running. Start one with /start <name>");
}

void server::ip(dpp::cluster& bot, const dpp::slashcommand_t& event)
{
    if (!active_container.empty())
    {
        dpp::user user = event.command.get_issuing_user();
        auto guild = event.command.get_guild();
        dpp::guild_member user_guild = guild.members[user.id];
        auto roles = user_guild.get_roles();
        bool chill = false;
        for (auto role: roles)
        {
            if (role.str() == "718598319537913859")
            {
                chill = true;
                break;
            }
        }

        if (chill)
        {
            bot.direct_message_create(user.id, dpp::message("Here's the ip for the " + active_container + " server\n" + getIP() + ":" + getPort()),
            [event](const dpp::confirmation_callback_t& callback)
            {
                if (callback.is_error())
                    event.reply("Smth went wrong. Could not slide into your DM");
                else
                    event.reply("Check your DM pookie");
            });
        }
        else
            event.reply("You're not allowed to see my ip");
    }
    else
        event.reply("No active server");
}