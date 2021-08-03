#include <stddef.h>
#include <elf.h>

#include "ldso.h"
#include "env.h"

static char** envp;
size_t pagesz;

void setenv(char** vector) { envp = vector; }

static char* env_match(char* env, const char* name)
{
    size_t count = 0;

    while (env[count] == name[count] && name[count] != '\0')
        count++;

    if (name[count] == '\0' && env[count] == '=') return env + count + 1;

    return NULL;
}

const char* env_get(const char* name)
{
    char** p = envp;

    if (name == NULL || name[0] == '\0') return NULL;

    for (; p[0] != NULL; p++) {
        char* val = env_match(p[0], name);
        if (val != NULL) {
            if (val[0] == '\0') val = NULL;
            return val;
        }
    }
    return NULL;
}

void parse_auxv(char* envp[], struct so_info* si, int show_auxv, char** base)
{
    ElfW(auxv_t) * auxv;

    *base = (char*)-1;

    while (*envp++ != NULL)
        ;

    for (auxv = (ElfW(auxv_t)*)envp; auxv->a_type != AT_NULL; auxv++) {
#define PARSE_ENTRY(type, hex)                                \
    if (auxv->a_type == type) {                               \
        if (show_auxv)                                        \
            xprintf(hex ? "%s:\t0x%x\n" : "%s:\t%d\n", #type, \
                    auxv->a_un.a_val);                        \
    }
#define PARSE_STR(type)                                                  \
    if (auxv->a_type == type) {                                          \
        if (show_auxv)                                                   \
            xprintf("%s:\t%s\n", (char*)#type, (char*)auxv->a_un.a_val); \
    }
#define PARSE_ENTRY_SET(type, hex, var)                       \
    if (auxv->a_type == type) {                               \
        var = (typeof(var))auxv->a_un.a_val;                  \
        if (show_auxv)                                        \
            xprintf(hex ? "%s:\t0x%x\n" : "%s:\t%d\n", #type, \
                    auxv->a_un.a_val);                        \
    }

        PARSE_ENTRY_SET(AT_PAGESZ, 0, pagesz);
        PARSE_ENTRY(AT_CLKTCK, 0);
        PARSE_ENTRY_SET(AT_PHDR, 1, si->phdr);
        PARSE_ENTRY_SET(AT_BASE, 1, *base);
        PARSE_ENTRY_SET(AT_PHNUM, 0, si->phnum);
        PARSE_ENTRY_SET(AT_ENTRY, 1, si->entry);
        PARSE_ENTRY(AT_UID, 0);
        PARSE_ENTRY(AT_GID, 0);
        PARSE_ENTRY(AT_EUID, 0);
        PARSE_ENTRY(AT_EGID, 0);
        PARSE_ENTRY(AT_SYSINFO, 1);
        PARSE_STR(AT_EXECFN);
        PARSE_STR(AT_PLATFORM);
    }
}
