/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
PUBLIC void * k_stacks;

PRIVATE char * env_get(const char *name);
PRIVATE int kinfo_set_param(char * buf, char * name, char * value);

PUBLIC void cstart(int argc, char* argv[]);

PUBLIC void cstart(int argc, char* argv[])
{
    char* a = 0x49020000;
    *a = 'o';
    while(1);
    memset(kinfo.cmdline, 0, sizeof(kinfo.cmdline));
    int i;
    for (i = 1; i < argc; i++) {
        char* arg = argv[i];
        direct_print("%s\n", arg);
    }
}

PRIVATE char * get_value(const char * param, const char * key)
{
    char * envp = (char *)param;
    const char * name = key;

    for (; *envp != 0;) {
        for (name = key; *name != 0 && *name == *envp; name++, envp++);
        if (*name == '\0' && *envp == '=') return envp + 1;
        while (*envp++ != 0);
    }

    return NULL;
}

PRIVATE char * env_get(const char *name)
{
    return get_value(kinfo.cmdline, name);
}

PRIVATE int kinfo_set_param(char * buf, char * name, char * value)
{
    char *p = buf;
    char *bufend = buf + KINFO_CMDLINE_LEN;
    char *q;
    int namelen = strlen(name);
    int valuelen = strlen(value);

    while (*p) {
        if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
            q = p;
            while (*q) q++;
            for (q++; q < bufend; q++, p++)
                *p = *q;
            break;
        }
        while (*p++);
        p++;
    }
    
    for (p = buf; p < bufend && (*p || *(p + 1)); p++);
    if (p > buf) p++;
    
    if (p + namelen + valuelen + 3 > bufend)
        return -1;
    
    strcpy(p, name);
    p[namelen] = '=';
    strcpy(p + namelen + 1, value);
    p[namelen + valuelen + 1] = 0;
    p[namelen + valuelen + 2] = 0;
    return 0;
}

PUBLIC void init_arch()
{
    
}
