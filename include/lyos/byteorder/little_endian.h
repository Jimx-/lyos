#ifndef _BYTEORDER_LITTLE_ENDIAN_H_
#define _BYTEORDER_LITTLE_ENDIAN_H_

#include <lyos/swab.h>

static inline u32 __be32_to_cpup(const __be32* p) { return (u32)__swab32p(p); }
static inline __be32 __cpu_to_be32p(const u32* p)
{
    return (__be32)__swab32p(p);
}

static inline u32 __be32_to_cpu(const __be32 p) { return (u32)__swab32p(&p); }
static inline __be32 __cpu_to_be32(const u32 p)
{
    return (__be32)__swab32p(&p);
}

#define __le16_to_cpu(x) ((__u16)(__le16)(x))
#define __le32_to_cpu(x) ((__u32)(__le32)(x))
#define __le64_to_cpu(x) ((__u64)(__le64)(x))
#define __cpu_to_le16(x) ((__le16)(__u16)(x))
#define __cpu_to_le32(x) ((__le32)(__u32)(x))
#define __cpu_to_le64(x) ((__le64)(__u64)(x))

#include <lyos/byteorder/generic.h>

#endif // _BYTEORDER_LITTLE_ENDIAN_H_
