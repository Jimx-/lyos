#include <stdio.h>
#include <sys/stat.h>

#include "proto.h"

#include "xlat/modetypes.h"

void print_mode_t(unsigned int mode)
{
    const char* fmt = NULL;

    if (mode & S_IFMT) {
        fmt = xlookup(&modetypes, mode & S_IFMT);
    }

    if (!fmt) {
        printf("%03o", mode);
        return;
    }

    printf("%s%s%s%s%s%#03o", fmt, fmt[0] ? "|" : "",
           (mode & S_ISUID) ? "S_ISUID|" : "",
           (mode & S_ISGID) ? "S_ISGID|" : "",
           (mode & S_ISVTX) ? "S_ISVTX|" : "",
           mode & ~(S_IFMT | S_ISUID | S_ISGID | S_ISVTX));
}
