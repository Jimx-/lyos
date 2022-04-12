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

    FIX_CPU_RELEASE_ADDR,

    FIX_PTE,
    FIX_PMD,
    FIX_PUD,
    FIX_PGD,

    __end_of_fixed_addresses
};

#define FIXADDR_SIZE  (__end_of_permanent_fixed_addresses << ARCH_PG_SHIFT)
#define FIXADDR_START (FIXADDR_TOP - FIXADDR_SIZE)

#define FIXMAP_PAGE_NORMAL ARM64_PG_KERNEL
#define FIXMAP_PAGE_IO     __pgprot(_ARM64_DEVICE_nGnRE)

extern void early_fixmap_init(void);

extern void __set_fixmap(enum fixed_address idx, phys_bytes phys,
                         pgprot_t prot);

#include <asm-generic/fixmap.h>

#endif
