#include <stddef.h>

size_t strlen(const char* str)
{
    int len = 0;
    while ((*str++) != '\0')
        len++;
    return len;
}

size_t strnlen(const char* s, size_t count)
{
    const char* sc;
    for (sc = s; *sc != '\0' && count--; ++sc)
        ;
    return sc - s;
}
