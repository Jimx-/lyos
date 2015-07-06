#ifndef _ENV_H_
#define _ENV_H_

void setenv(char** vector);
const char* env_get(const char* name);
void parse_auxv(char* envp[], struct so_info* si, int show_auxv, char** base);

#endif
