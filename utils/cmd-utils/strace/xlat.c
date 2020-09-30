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
