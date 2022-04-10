#ifndef _ASM_GENERIC_FIXMAP_H_
#define _ASM_GENERIC_FIXMAP_H_

#define __fix_to_virt(x) (FIXADDR_TOP - ((x) << ARCH_PG_SHIFT))

#endif
