#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define U16_F PRIu16
#define S16_F PRId16
#define X16_F PRIx16
#define U32_F PRIu32
#define S32_F PRId32
#define X32_F PRIx32
#define SZT_F "zu"

#define PACK_STRUCT_STRUCT __packed

#ifdef NDEBUG
#define LWIP_NOASSERT
#else
#define LWIP_PLATFORM_ASSERT(x) assert(x)
#endif

#define LWIP_PLATFORM_DIAG(x) \
    do {                      \
        printl x;             \
    } while (0)

extern uint32_t lwip_hook_rand(void);

#define LWIP_RAND lwip_hook_rand

#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

#define lwip_htons htons
#define lwip_htonl htonl

extern int lwip_ip4_forward;
extern int lwip_ip6_forward;

#endif /* !LWIP_ARCH_CC_H */
