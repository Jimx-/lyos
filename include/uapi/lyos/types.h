#ifndef _UAPI_LYOS_TYPES_H_
#define _UAPI_LYOS_TYPES_H_

#include <asm/types.h>

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;

typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef int __endpoint_t;

typedef __u32 __mgrant_id_t;

typedef unsigned __poll_t;

#endif
