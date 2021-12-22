//
// vsprintf.c
//
// Print formatting routines
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <stdarg.h>
#include <sys/types.h>
#include <limits.h>

#define ZEROPAD 1  // Pad with zero
#define SIGN    2  // Unsigned/signed long
#define PLUS    4  // Show plus
#define SPACE   8  // Space if plus
#define LEFT    16 // Left justified
#define SPECIAL 32 // 0x
#define LARGE   64 // Use 'ABCDEF' instead of 'abcdef'

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
static char* upper_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

long strtol(const char* nptr, char** endptr, int base)
{
    const unsigned char* s = (const unsigned char*)nptr;
    unsigned long acc = 0;
    int c;
    unsigned long cutoff;
    int neg = 0, any = 0, cutlim;

    do {
        c = *s++;
    } while (c == ' ');
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+')
        c = *s++;
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) base = c == '0' ? 8 : 10;

    cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
    cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;
    for (acc = 0, any = 0;; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
    } else if (neg)
        acc = -acc;
    if (endptr != 0) *endptr = (char*)(any ? (char*)s - 1 : nptr);
    return (acc);
}

static size_t strnlen(const char* s, size_t count)
{
    const char* sc;
    for (sc = s; *sc != '\0' && count--; ++sc)
        ;
    return sc - s;
}

static int skip_atoi(const char** s)
{
    int i = 0;
    while (is_digit(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

static char* number(char* str, char* end, long num, int base, int size,
                    int precision, int type)
{
    char c, sign, tmp[66];
    char* dig = digits;
    int i;

    if (type & LARGE) dig = upper_digits;
    if (type & LEFT) type &= ~ZEROPAD;
    if (base < 2 || base > 36) return 0;

    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    if (type & SIGN) {
        if (num < 0) {
            sign = '-';
            num = -num;
            size--;
        } else if (type & PLUS) {
            sign = '+';
            size--;
        } else if (type & SPACE) {
            sign = ' ';
            size--;
        }
    }

    if (type & SPECIAL) {
        if (base == 16) {
            size -= 2;
        } else if (base == 8) {
            size--;
        }
    }

    i = 0;

    if (num == 0) {
        tmp[i++] = '0';
    } else {
        while (num != 0) {
            tmp[i++] = dig[((unsigned long)num) % (unsigned)base];
            num = ((unsigned long)num) / (unsigned)base;
        }
    }

    if (i > precision) precision = i;
    size -= precision;
    if (!(type & (ZEROPAD | LEFT)))
        while (size-- > 0) {
            if (str < end) *str = ' ';
            str++;
        }
    if (sign) {
        if (str < end) *str = sign;
        str++;
    }

    if (type & SPECIAL) {
        if (base == 8) {
            if (str < end) *str = '0';
            str++;
        } else if (base == 16) {
            if (str < end) *str = '0';
            str++;
            if (str < end) *str = digits[33];
            str++;
        }
    }

    if (!(type & LEFT))
        while (size-- > 0) {
            if (str < end) *str = c;
            str++;
        }
    while (i < precision--) {
        if (str < end) *str = '0';
        str++;
    }
    while (i-- > 0) {
        if (str < end) *str = tmp[i];
        str++;
    }
    while (size-- > 0) {
        if (str < end) *str = ' ';
        str++;
    }

    return str;
}

static char* eaddr(char* str, char* end, unsigned char* addr, int size,
                   int precision, int type)
{
    char tmp[24];
    char* dig = digits;
    int i, len;

    if (type & LARGE) dig = upper_digits;
    len = 0;
    for (i = 0; i < 6; i++) {
        if (i != 0) tmp[len++] = ':';
        tmp[len++] = dig[addr[i] >> 4];
        tmp[len++] = dig[addr[i] & 0x0F];
    }

    if (!(type & LEFT))
        while (len < size--) {
            if (str < end) *str = ' ';
            str++;
        }
    for (i = 0; i < len; ++i, ++str) {
        if (str < end) *str = tmp[i];
    }
    while (len < size--) {
        if (str < end) *str = ' ';
        str++;
    }

    return str;
}

static char* iaddr(char* str, char* end, unsigned char* addr, int size,
                   int precision, int type)
{
    char tmp[24];
    int i, n, len;

    len = 0;
    for (i = 0; i < 4; i++) {
        if (i != 0) tmp[len++] = '.';
        n = addr[i];

        if (n == 0) {
            tmp[len++] = digits[0];
        } else {
            if (n >= 100) {
                tmp[len++] = digits[n / 100];
                n = n % 100;
                tmp[len++] = digits[n / 10];
                n = n % 10;
            } else if (n >= 10) {
                tmp[len++] = digits[n / 10];
                n = n % 10;
            }

            tmp[len++] = digits[n];
        }
    }

    if (!(type & LEFT))
        while (len < size--) {
            if (str < end) *str = ' ';
            str++;
        }
    for (i = 0; i < len; ++i, ++str) {
        if (str < end) *str = tmp[i];
    }
    while (len < size--) {
        if (str < end) *str = ' ';
        str++;
    }

    return str;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    int len;
    unsigned long num;
    int i, base;
    char *str, *end;
    char* s;

    int flags; // Flags to number()

    int field_width; // Width of output field
    int precision;   // Min. # of digits for integers; max number of chars for
                     // from string
    int qualifier;   // 'h', 'l', or 'L' for integer fields

    str = buf;
    end = buf + size;

    if (end < buf) {
        end = ((void*)-1);
        size = end - buf;
    }

    for (str = buf; *fmt; fmt++) {
        if (*fmt != '%') {
            if (str < end) *str = *fmt;
            str++;
            continue;
        }

        // Process flags
        flags = 0;
    repeat:
        fmt++; // This also skips first '%'
        switch (*fmt) {
        case '-':
            flags |= LEFT;
            goto repeat;
        case '+':
            flags |= PLUS;
            goto repeat;
        case ' ':
            flags |= SPACE;
            goto repeat;
        case '#':
            flags |= SPECIAL;
            goto repeat;
        case '0':
            flags |= ZEROPAD;
            goto repeat;
        }

        // Get field width
        field_width = -1;
        if (is_digit(*fmt)) {
            field_width = skip_atoi(&fmt);
        } else if (*fmt == '*') {
            fmt++;
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        // Get the precision
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (is_digit(*fmt)) {
                precision = skip_atoi(&fmt);
            } else if (*fmt == '*') {
                ++fmt;
                precision = va_arg(args, int);
            }
            if (precision < 0) precision = 0;
        }

        // Get the conversion qualifier
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt++;
            if (qualifier == *fmt) {
                if (qualifier == 'h') {
                    qualifier = 'H';
                    fmt++;
                } else if (qualifier == 'l') {
                    qualifier = 'L';
                    fmt++;
                }
            }
        }

        // Default base
        base = 10;

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT))
                while (--field_width > 0) {
                    if (str < end) *str = ' ';
                    str++;
                }

            if (str < end) *str = (unsigned char)va_arg(args, int);
            str++;

            while (--field_width > 0) {
                if (str < end) *str = ' ';
                str++;
            }
            continue;

        case 's':
            s = va_arg(args, char*);
            if (!s) s = "<NULL>";
            len = strnlen(s, precision);
            if (!(flags & LEFT))
                while (len < field_width--) {
                    if (str < end) *str = ' ';
                    str++;
                }
            for (i = 0; i < len; ++i, ++s, ++str) {
                if (str < end) *str = *s;
            }

            while (len < field_width--) {
                if (str < end) *str = ' ';
                str++;
            }
            continue;

        case 'p':
            if (field_width == -1) {
                field_width = 2 * sizeof(void*);
                flags |= ZEROPAD;
            }
            str = number(str, end, (unsigned long)va_arg(args, void*), 16,
                         field_width, precision, flags);
            continue;

        case 'n':
            if (qualifier == 'l') {
                long* ip = va_arg(args, long*);
                *ip = (str - buf);
            } else {
                int* ip = va_arg(args, int*);
                *ip = (str - buf);
            }
            continue;

        case 'A':
            flags |= LARGE;

        case 'a':
            if (qualifier == 'l') {
                str = eaddr(str, end, va_arg(args, unsigned char*), field_width,
                            precision, flags);
            } else {
                str = iaddr(str, end, va_arg(args, unsigned char*), field_width,
                            precision, flags);
            }
            continue;

        // Integer number formats - set up the flags and "break"
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;

        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;

        case 'u':
            break;

        default:
            if (*fmt != '%') *str++ = '%';
            if (*fmt) {
                if (str < end) *str = *fmt;
                str++;
            } else {
                --fmt;
            }
            continue;
        }

        if (qualifier == 'l') {
            num = va_arg(args, unsigned long);
        } else if (qualifier == 'L') {
            num = va_arg(args, unsigned long long);
        } else if (qualifier == 'h') {
            if (flags & SIGN) {
                num = va_arg(args, int);
            } else {
                num = va_arg(args, unsigned int);
            }
        } else if (flags & SIGN) {
            num = va_arg(args, int);
        } else {
            num = va_arg(args, unsigned int);
        }

        str = number(str, end, num, base, field_width, precision, flags);
    }

    if (size > 0) {
        if (str < end)
            *str = '\0';
        else
            end[-1] = '\0';
    }

    return str - buf;
}

int vsprintf(char* buf, const char* fmt, va_list args)
{
    return vsnprintf(buf, INT_MAX, fmt, args);
}

int sprintf(char* buf, const char* fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsprintf(buf, fmt, args);
    va_end(args);

    return n;
}

int snprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}
