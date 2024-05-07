#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parseENV.h"

void remove_newline(char *line)
{
    if (*line == '\0')
        return;

    while (*line != '\0')
        line++; // inc until end
    line--; // dec once to get last char
    if (*line == '\n')
        *line = '\0'; // remove newline if exists
}

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
                remove_newline(value);
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