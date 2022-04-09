#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#if CONFIG_PGTABLE_LEVELS == 2
#include <lyos/pagetable-nopmd.h>
#elif CONFIG_PGTABLE_LEVELS == 3
#include <lyos/pagetable-nopud.h>
#endif

#define _ARM64_SECT_BASE ()
#endif
