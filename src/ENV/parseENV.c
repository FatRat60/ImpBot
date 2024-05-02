#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parseENV.h"

void parse_env()
{
    FILE *env_file;
    env_file = fopen(ENV_PATH, "r");
    // parse contents
    if (env_file != NULL)
    {
        char *line = NULL;
        size_t len = 0;
        size_t line_number = 1;
        while (getline(&line, &len, env_file) != -1)
        {
            char *name = strtok(line, "=");
            char *value = strtok(NULL, "=");
            if (value != NULL)
            {
                setenv(name, value, 0);
            }
            else
            {
                printf("Line %ld failed to parse\n", line_number);
            }
        }
        free(line);
        fclose(env_file);
    }
}

char * get_env_var(const char *var_name)
{
    return getenv(var_name);
}