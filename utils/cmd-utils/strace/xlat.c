#include <stdio.h>

#include "xlat.h"

const char* xlookup(const struct xlat* xlat, uint64_t val)
{
    int i;

    for (i = 0; i < xlat->size; i++) {
        if (xlat->data[i].val == val) return xlat->data[i].str;
    }

    return NULL;
}

int print_xval(uint64_t val, const struct xlat* xlat)
{
    const char* str;

    str = xlookup(xlat, val);

    if (str)
        printf("%s", str);
    else
        printf("%lld", val);

    return !!str;
}

int print_flags(uint64_t flags, const struct xlat* xlat)
{
    int i;
    const char* init_sep = "";
    unsigned int n = 0;

    for (i = 0; i < xlat->size; i++) {
        uint64_t v = xlat->data[i].val;

        if (xlat->data[i].str && ((flags == v) || (v && (flags & v) == v))) {
            printf("%s%s", (n++ ? "|" : init_sep), xlat->data[i].str);
            flags &= ~v;
        }

        if (!flags) break;
    }

    if (n) {
        if (flags) {
            printf("|%x", flags);
            n++;
        }
    } else {
        printf("0");
    }

    return n;
}

const char* sprint_flags(uint64_t flags, const struct xlat* xlat)
{
    int i;
    const char* init_sep = "";
    unsigned int n = 0;
    static char buf[1024];
    char* outptr = buf;

    for (i = 0; i < xlat->size; i++) {
        uint64_t v = xlat->data[i].val;

        if (xlat->data[i].str && ((flags == v) || (v && (flags & v) == v))) {
            outptr += sprintf(outptr, "%s%s", (n++ ? "|" : init_sep),
                              xlat->data[i].str);
            flags &= ~v;
        }

        if (!flags) break;
    }

    if (n) {
        if (flags) {
            outptr += sprintf(outptr, "|%x", flags);
            n++;
        }
    } else {
        *outptr++ = '0';
    }

    *outptr++ = '\0';

    return buf;
}
