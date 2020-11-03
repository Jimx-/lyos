#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <machine/endian.h>

#if _BYTE_ORDER == LITTLE_ENDIAN
#define le16toh(x) (uint16_t)(x)
#else
#define le16toh(x) __bswap16(_x)
#endif

#endif
