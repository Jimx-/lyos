#ifndef _LIBOF_H_
#define _LIBOF_H_

#include <asm/byteorder.h>

PRIVATE inline u64 of_read_number(const u32* cell, int size)
{
    u64 r = 0;
    while (size--) {
        r = (r << 32) | be32_to_cpup(cell++);
    }
    return r;
}

PUBLIC int of_scan_fdt(int (*scan)(void*, unsigned long, const char*, int,
                                   void*),
                       void* arg, void* blob);

#endif // _LIBOF_H_
