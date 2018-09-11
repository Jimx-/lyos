#ifndef _ARCH_PAGE_64_H_
#define _ARCH_PAGE_64_H_

#define RISCV_PGD_SHIFT   (30)
#define RISCV_PGD_SIZE    (1UL << RISCV_PGD_SHIFT)
#define RISCV_PGD_MASK    (~(RISCV_PGD_SIZE - 1))

#define RISCV_PMD_SHIFT   (21)
#define RISCV_PMD_SIZE    (1UL << RISCV_PMD_SHIFT)
#define RISCV_PMD_MASK    (~(RISCV_PMD_SIZE - 1))

typedef struct {
    unsigned long   pmd;
} pmd_t;

#define pmd_val(x)  ((x).pmd)
#define __pmd(x)    ((pmd_t) { (x) })

#define RISCV_VM_PMD_ENTRIES     (ARCH_PG_SIZE / sizeof(pmd_t))

#endif // _ARCH_PAGE_64_H_
