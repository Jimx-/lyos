#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

static int inet_pton4(const char* src, unsigned char* dst)
{
    unsigned char tmp[4];
    unsigned int value = 0;
    int octet = 0;
    int digits = 0;

    while (*src) {
        if (*src >= '0' && *src <= '9') {
            value = value * 10 + (*src - '0');
            if (value > 255 || ++digits > 3) return 0;
        } else if (*src == '.' && digits && octet < 3) {
            tmp[octet++] = value;
            value = 0;
            digits = 0;
        } else {
            return 0;
        }
        src++;
    }

    if (!digits || octet != 3) return 0;
    tmp[octet] = value;
    memcpy(dst, tmp, sizeof(tmp));
    return 1;
}

static int hex_value(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int inet_pton6(const char* src, unsigned char* dst)
{
    unsigned char tmp[16] = {0};
    unsigned char* tp = tmp;
    unsigned char* end = tmp + sizeof(tmp);
    unsigned char* colon = NULL;
    const char* token;
    unsigned int value = 0;
    int digits = 0;
    int ch;

    if (*src == ':' && *++src != ':') return 0;
    token = src;

    while ((ch = *src++) != '\0') {
        int hex = hex_value(ch);

        if (hex >= 0) {
            if (++digits > 4) return 0;
            value = (value << 4) | hex;
            continue;
        }

        if (ch == ':') {
            token = src;
            if (!digits) {
                if (colon) return 0;
                colon = tp;
                continue;
            }
            if (*src == '\0' || tp + 2 > end) return 0;
            *tp++ = value >> 8;
            *tp++ = value;
            digits = 0;
            value = 0;
            continue;
        }

        if (ch == '.' && digits && tp + 4 <= end && inet_pton4(token, tp)) {
            tp += 4;
            digits = 0;
            break;
        }

        return 0;
    }

    if (digits) {
        if (tp + 2 > end) return 0;
        *tp++ = value >> 8;
        *tp++ = value;
    }

    if (colon) {
        size_t tail = tp - colon;
        if (tp == end) return 0;
        memmove(end - tail, colon, tail);
        memset(colon, 0, end - tail - colon);
        tp = end;
    }

    if (tp != end) return 0;
    memcpy(dst, tmp, sizeof(tmp));
    return 1;
}

int inet_pton(int af, const char* restrict src, void* restrict dst)
{
    switch (af) {
    case AF_INET:
        return inet_pton4(src, dst);
    case AF_INET6:
        return inet_pton6(src, dst);
    default:
        errno = EAFNOSUPPORT;
        return -1;
    }
}
