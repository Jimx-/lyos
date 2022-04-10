#ifndef _ARCH_FIXMAP_H_
#define _ARCH_FIXMAP_H_

#include <asm/page.h>

enum fixed_address {
    FIX_HOLE,

#define FIX_FDT_SIZE (4 << 20)
    FIX_FDT_END,
    FIX_FDT = FIX_FDT_END + (FIX_FDT_SIZE >> ARCH_PG_SHIFT) - 1,

    FIX_EARLYCON_MEM_BASE,

    __end_of_permanent_fixed_addresses,
};

#define FIXADDR_SIZE  (__end_of_permanent_fixed_addresses << ARCH_PG_SHIFT)
#define FIXADDR_START (FIXADDR_TOP - FIXADDR_SIZE)

extern void early_fixmap_init(void);

#include <asm-generic/fixmap.h>

#endif
