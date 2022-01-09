#include <arpa/inet.h>
#include <machine/endian.h>

uint16_t htons(uint16_t x)
{
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return __bswap16(x);
#else
    return x;
#endif
}

uint16_t ntohs(uint16_t x)
{
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return __bswap16(x);
#else
    return x;
#endif
}

uint32_t htonl(uint32_t x)
{
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return __bswap32(x);
#else
    return x;
#endif
}

uint32_t ntohl(uint32_t x)
{
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return __bswap32(x);
#else
    return x;
#endif
}
