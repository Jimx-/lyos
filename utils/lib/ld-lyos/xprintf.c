#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#define STR_DEFAULT_LEN 512

static char* i2a(int val, int base, char** ps)
{
    int m = val % base;
    int q = val / base;
    if (q) {
        i2a(q, base, ps);
    }
    *(*ps)++ = (m < 10) ? (m + '0') : (m - 10 + 'a');

    return *ps;
}

static char* u2a(unsigned int val, int base, char** ps)
{
    unsigned int m = val % base;
    unsigned int q = val / base;
    if (q) {
        i2a(q, base, ps);
    }
    *(*ps)++ = (m < 10) ? (m + '0') : (m - 10 + 'a');

    return *ps;
}

static int vsprintf(char* buf, const char* fmt, char* args)
{
    char* p;

    va_list p_next_arg = args;
    int m;

    char inner_buf[STR_DEFAULT_LEN];
    char cs;
    int align_nr;
    int len;

    for (p = buf; *fmt; fmt++) {
        if (*fmt != '%') {
            *p++ = *fmt;
            continue;
        } else { /* a format string begins */
            align_nr = 0;
        }

        fmt++;

        if (*fmt == '%') {
            *p++ = *fmt;
            continue;
        } else if (*fmt == '0') {
            cs = '0';
            fmt++;
        } else {
            cs = ' ';
        }
        while (((unsigned char)(*fmt) >= '0') &&
               ((unsigned char)(*fmt) <= '9')) {
            align_nr *= 10;
            align_nr += *fmt - '0';
            fmt++;
        }

        char* q = inner_buf;
        memset(q, 0, sizeof(inner_buf));

        switch (*fmt) {
        case 'c':
            *q++ = *((char*)p_next_arg);
            p_next_arg += 4;
            break;
        case 'x':
            m = *((int*)p_next_arg);
            u2a(m, 16, &q);
            p_next_arg += 4;
            break;
        case 'd':
            m = *((int*)p_next_arg);
            if (m < 0) {
                m = m * (-1);
                *q++ = '-';
            }
            i2a(m, 10, &q);
            p_next_arg += 4;
            break;
        case 's':
            len = strlen(*((char**)p_next_arg));
            memcpy(q, (*((char**)p_next_arg)), len);
            q += len;
            p_next_arg += 4;
            break;
        default:
            break;
        }

        int k;
        for (k = 0; k < ((align_nr > strlen(inner_buf))
                             ? (align_nr - strlen(inner_buf))
                             : 0);
             k++) {
            *p++ = cs;
        }
        q = inner_buf;
        while (*q) {
            *p++ = *q++;
        }
    }

    *p = 0;

    return (p - buf);
}

int xprintf(const char* fmt, ...)
{
    int i;
    char buf[STR_DEFAULT_LEN];
    va_list arg;

    va_start(arg, fmt);
    i = vsprintf(buf, fmt, arg);
    int c = write(1, buf, i);
    va_end(arg);

    return i;
}
