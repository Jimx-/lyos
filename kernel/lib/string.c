#include <asm/string.h>
#include <string.h>

#ifndef __HAVE_ARCH_STRLEN
size_t strlen(const char* s)
{
    const char* sc;

    for (sc = s; *sc != '\0'; ++sc)
        ;

    return sc - s;
}
#endif

#ifndef __HAVE_ARCH_STRNLEN
size_t strnlen(const char* s, size_t count)
{
    const char* sc;

    for (sc = s; count-- && *sc != '\0'; ++sc)
        ;
    return sc - s;
}
#endif

#ifndef __HAVE_ARCH_STRCPY
char* strcpy(char* dest, const char* src)
{
    char* tmp = dest;

    while ((*dest++ = *src++) != '\0')
        ;

    return tmp;
}
#endif

#ifndef __HAVE_ARCH_STRCMP
int strcmp(const char* cs, const char* ct)
{
    unsigned char c1, c2;

    while (1) {
        c1 = *cs++;
        c2 = *ct++;
        if (c1 != c2) return c1 < c2 ? -1 : 1;
        if (!c1) break;
    }
    return 0;
}
#endif

#ifndef __HAVE_ARCH_STRNCMP
int strncmp(const char* cs, const char* ct, size_t count)
{
    unsigned char c1, c2;

    while (count) {
        c1 = *cs++;
        c2 = *ct++;
        if (c1 != c2) return c1 < c2 ? -1 : 1;
        if (!c1) break;
        count--;
    }
    return 0;
}
#endif

#ifndef __HAVE_ARCH_STRLCPY
size_t strlcpy(char* dest, const char* src, size_t size)
{
    size_t ret = strlen(src);

    if (size) {
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return ret;
}
#endif

#ifndef __HAVE_ARCH_STRNCPY
char* strncpy(char* dest, const char* src, size_t count)
{
    char* tmp = dest;

    while (count) {
        if ((*tmp = *src) != 0) src++;
        tmp++;
        count--;
    }
    return dest;
}
#endif

#ifndef __HAVE_ARCH_STRRCHR
char* strrchr(const char* s, int c)
{
    const char* last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}
#endif

#ifndef __HAVE_ARCH_MEMSET
void* memset(void* s, int c, size_t count)
{
    char* xs = s;

    while (count--)
        *xs++ = c;
    return s;
}
#endif

#ifndef __HAVE_ARCH_MEMCMP
int memcmp(const void* cs, const void* ct, size_t count)
{
    const unsigned char *su1, *su2;
    int res = 0;

    for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0) break;
    return res;
}
#endif

#ifndef __HAVE_ARCH_MEMCPY
void* memcpy(void* dest, const void* src, size_t count)
{
    char* tmp = dest;
    const char* s = src;

    while (count--)
        *tmp++ = *s++;
    return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMCHR
void* memchr(const void* s, int c, size_t n)
{
    const unsigned char* p = s;
    while (n-- != 0) {
        if ((unsigned char)c == *p++) {
            return (void*)(p - 1);
        }
    }
    return NULL;
}
#endif

#ifndef __HAVE_ARCH_MEMMOVE
void* memmove(void* dest, const void* src, size_t count)
{
    char* tmp;
    const char* s;

    if (dest <= src) {
        tmp = dest;
        s = src;
        while (count--)
            *tmp++ = *s++;
    } else {
        tmp = dest;
        tmp += count;
        s = src;
        s += count;
        while (count--)
            *--tmp = *--s;
    }
    return dest;
}
#endif
