#include <iostream>
#include <string>
#include "ENV/parseENV.h"

int main(int, char**)
{
    parse_env();
    char * var;
    if ( (var = get_env_var("DISCORD_TOKEN")) != NULL)
    {
        std::string TOKEN(var);
        std::cout << "TOKEN: " << TOKEN << "\n";
    }
    return 0;
}