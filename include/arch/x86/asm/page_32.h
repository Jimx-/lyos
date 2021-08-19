#ifndef _ARCH_PAGE_32_H_
#define _ARCH_PAGE_32_H_

/* size of page directory */
#define I386_PGD_SHIFT (22)
#define I386_PGD_SIZE  (1UL << I386_PGD_SHIFT)
#define I386_PGD_MASK  (~(I386_PGD_SIZE - 1))

/* size of a virtual page */
#define I386_PG_SHIFT (12)
#define I386_PG_SIZE  (1UL << I386_PG_SHIFT)
#define I386_PG_MASK  (~(I386_PG_SIZE - 1))

#define I386_VM_DIR_ENTRIES 1024
#define I386_VM_PT_ENTRIES  1024

#define ARCH_PGD_SIZE  I386_PGD_SIZE
#define ARCH_PGD_SHIFT I386_PGD_SHIFT
#define ARCH_PGD_MASK  I386_PGD_MASK

#define ARCH_PG_SIZE  I386_PG_SIZE
#define ARCH_PG_SHIFT I386_PG_SHIFT
#define ARCH_PG_MASK  I386_PG_MASK

#define ARCH_VM_DIR_ENTRIES I386_VM_DIR_ENTRIES
#define ARCH_VM_PT_ENTRIES  I386_VM_PT_ENTRIES

#define ARCH_PDE(x) ((unsigned long)(x) >> I386_PGD_SHIFT & 0x03FF)
#define ARCH_PTE(x) ((unsigned long)(x) >> I386_PG_SHIFT & 0x03FF)

#endif
