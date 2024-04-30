#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include "string.h"
#include "parseENV.h"

int parse_env()
{
    std::ifstream env_file(ENV_PATH);
    // parse contents
    if (env_file.is_open())
    {
        std::string line;
        size_t line_number = 1;
        while (std::getline(env_file, line))
        {
            if (putenv(line.c_str()) != 0)
            {
                std::cerr << "Line " << line_number << " failed to parse\n";
            }
        }
    }
    return env_file.is_open();
}

char * get_env_var(const char *var_name)
{
    return getenv(var_name);
}