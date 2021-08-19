#ifndef _ARCH_PAGE_64_H_
#define _ARCH_PAGE_64_H_

/* size of page directory */
#define ARCH_PGD_SHIFT (39)
#define ARCH_PGD_SIZE  (1UL << ARCH_PGD_SHIFT)
#define ARCH_PGD_MASK  (~(ARCH_PGD_SIZE - 1))

/* size of a page upper directory */
#define ARCH_PUD_SHIFT (30)
#define ARCH_PUD_SIZE  (1UL << ARCH_PUD_SHIFT)
#define ARCH_PUD_MASK  (~(ARCH_PUD_SIZE - 1))

/* size of a page middle directory */
#define ARCH_PMD_SHIFT (21)
#define ARCH_PMD_SIZE  (1UL << ARCH_PMD_SHIFT)
#define ARCH_PMD_MASK  (~(ARCH_PMD_SIZE - 1))

/* size of a virtual page */
#define ARCH_PG_SHIFT (12)
#define ARCH_PG_SIZE  (1UL << ARCH_PG_SHIFT)
#define ARCH_PG_MASK  (~(ARCH_PG_SIZE - 1))

#define ARCH_VM_DIR_ENTRIES 512
#define ARCH_VM_PUD_ENTRIES 512
#define ARCH_VM_PMD_ENTRIES 512
#define ARCH_VM_PT_ENTRIES  512

#endif
