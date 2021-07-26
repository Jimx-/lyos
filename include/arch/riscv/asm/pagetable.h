#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#include <asm/pagetable_bits.h>

#define _RISCV_PG_BASE \
    (_RISCV_PG_PRESENT | _RISCV_PG_ACCESSED | _RISCV_PG_USER | _RISCV_PG_DIRTY)

#define RISCV_PG_NONE __pgprot(0)
#define RISCV_PG_READ __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ)
#define RISCV_PG_WRITE \
    __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_WRITE)
#define RISCV_PG_EXEC __pgprot(_RISCV_PG_BASE | _RISCV_PG_EXEC)
#define RISCV_PG_EXEC_READ \
    __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_EXEC)
#define RISCV_PG_EXEC_WRITE \
    __pgprot(_RISCV_PG_BASE | _RISCV_PG_READ | _RISCV_PG_WRITE | _RISCV_PG_EXEC)

#define _RISCV_PG_KERNEL                                    \
    (_RISCV_PG_PRESENT | _RISCV_PG_READ | _RISCV_PG_WRITE | \
     _RISCV_PG_ACCESSED | _RISCV_PG_DIRTY)
#define RISCV_PG_KERNEL      __pgprot(_RISCV_PG_KERNEL)
#define RISCV_PG_KERNLE_EXEC __pgprot(_RISCV_PG_KERNEL | _RISCV_PG_EXEC)

static inline pde_t pfn_pde(unsigned long pfn, pgprot_t prot)
{
    return __pde((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

static inline pmd_t pfn_pmd(unsigned long pfn, pgprot_t prot)
{
    return __pmd((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
    return __pte((pfn << RISCV_PG_PFN_SHIFT) | pgprot_val(prot));
}

static inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

static inline int pde_present(pde_t pde)
{
    return pde_val(pde) & _RISCV_PG_PRESENT;
}

static inline void pde_populate(pde_t* pde, pmd_t* pmd)
{
    unsigned long pfn = __pa(pmd) >> ARCH_PG_SHIFT;
    *pde = __pde((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

static inline pmd_t* pmd_offset(pde_t* pmd, unsigned long addr)
{
    pmd_t* vaddr =
        (pmd_t*)__va((pde_val(*pmd) >> RISCV_PG_PFN_SHIFT) << ARCH_PG_SHIFT);
    return vaddr + ARCH_PMDE(addr);
}

static inline int pmde_present(pmd_t pmde)
{
    return pmd_val(pmde) & _RISCV_PG_PRESENT;
}

static inline void pmde_populate(pmd_t* pmde, pte_t* pt)
{
    unsigned long pfn = __pa(pt) >> ARCH_PG_SHIFT;
    *pmde = __pmd((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

static inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr =
        (pte_t*)__va((pmd_val(*pt) >> RISCV_PG_PFN_SHIFT) << ARCH_PG_SHIFT);
    return vaddr + ARCH_PTE(addr);
}

static inline int pte_present(pte_t pte)
{
    return pte_val(pte) & _RISCV_PG_PRESENT;
}

#endif // _ARCH_PAGETABLE_H_
