#ifndef _BYTEORDER_LITTLE_ENDIAN_H_
#define _BYTEORDER_LITTLE_ENDIAN_H_

#include <lyos/swab.h>

PRIVATE inline u32 __be32_to_cpup(const u32* p) { return (u32)__swab32p(p); }

#include <lyos/byteorder/generic.h>

#endif // _BYTEORDER_LITTLE_ENDIAN_H_
