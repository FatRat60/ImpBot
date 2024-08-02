#ifndef SERVER_H
#define SERVER_H

#include <dpp/dpp.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

#define SERVER_LIST_FILE "servers.txt"
#define MIKU_THUMBNAIL "https://assets.audiomack.com/ghxstnx/c31bf1a8c0b277651cb1e02630598e57afb31857ebec32992842e66de6bac31e.jpeg?width=300"

/*Will handle all functionality related to starting, stopping, and configuring game servers*/
class server
{
    public:
        static std::vector<std::string> get_available_servers();
        static void checkForActiveServer(dpp::cluster& bot);
        static void setDir(std::string dir) { server_dir = dir; }
        static bool dirExists() { return !server_dir.empty(); }
        static void start(const dpp::slashcommand_t& event);
        static void terminate(const dpp::slashcommand_t& event);
        static void ip(dpp::cluster& bot, const dpp::slashcommand_t& event);
    private:
        static std::string server_dir;
        static std::string active_container;
        static std::string getIP();
        static std::string getPort();
        static dpp::embed createServerEmbed();
};

#endif