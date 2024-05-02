#ifndef PARSEENV_H
#define PARSEENV_H

#define ENV_PATH "resources/.env"

void parse_env();
char * get_env_var(const char *var_name);

#endif