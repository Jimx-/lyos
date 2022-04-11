#ifndef _ASM_GENERIC_FIXMAP_H_
#define _ASM_GENERIC_FIXMAP_H_

#define __fix_to_virt(x) (FIXADDR_TOP - ((x) << ARCH_PG_SHIFT))

#ifndef FIXMAP_PAGE_CLEAR
#define FIXMAP_PAGE_CLEAR __pgprot(0)
#endif

#ifndef set_fixmap
#define set_fixmap(idx, phys) __set_fixmap(idx, phys, FIXMAP_PAGE_NORMAL)
#endif

#ifndef clear_fixmap
#define clear_fixmap(idx) __set_fixmap(idx, 0, FIXMAP_PAGE_CLEAR)
#endif

/* Return a pointer with offset calculated */
#define __set_fixmap_offset(idx, phys, flags)                              \
    ({                                                                     \
        unsigned long ________addr;                                        \
        __set_fixmap(idx, phys, flags);                                    \
        ________addr = __fix_to_virt(idx) + ((phys) & (ARCH_PG_SIZE - 1)); \
        ________addr;                                                      \
    })

#define set_fixmap_offset(idx, phys) \
    __set_fixmap_offset(idx, phys, FIXMAP_PAGE_NORMAL)

#endif
