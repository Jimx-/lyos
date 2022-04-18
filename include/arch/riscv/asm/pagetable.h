#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#include <asm/page.h>
#include <asm/pagetable_bits.h>

#include <lyos/pagetable-nopud.h>

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

/*         xwr */
#define __P000 RISCV_PG_NONE
#define __P001 RISCV_PG_READ
#define __P010 RISCV_PG_WRITE
#define __P011 RISCV_PG_WRITE
#define __P100 RISCV_PG_EXEC
#define __P101 RISCV_PG_EXEC_READ
#define __P110 RISCV_PG_EXEC_WRITE
#define __P111 RISCV_PG_EXEC_WRITE

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

static inline unsigned long pte_pfn(pte_t pte)
{
    return pte_val(pte) >> RISCV_PG_PFN_SHIFT;
}

static inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

static inline int pude_present(pud_t pude)
{
    return pud_val(pude) & _RISCV_PG_PRESENT;
}

static inline int pude_none(pud_t pude) { return pud_val(pude) == 0; }

static inline int pude_bad(pud_t pude) { return !pude_present(pude); }

static inline void pude_clear(pud_t* pude) { *pude = __pud(0); }

static inline void pude_populate(pud_t* pude, phys_bytes pmd_phys)
{
    unsigned long pfn = pmd_phys >> ARCH_PG_SHIFT;
    *pude = __pud((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

static inline pmd_t* pmd_offset(pud_t* pmd, unsigned long addr)
{
    pmd_t* vaddr = (pmd_t*)phys_to_virt((pud_val(*pmd) >> RISCV_PG_PFN_SHIFT)
                                        << ARCH_PG_SHIFT);
    return vaddr + ARCH_PMDE(addr);
}

static inline int pmde_present(pmd_t pmde)
{
    return pmd_val(pmde) & _RISCV_PG_PRESENT;
}

static inline int pmde_none(pmd_t pmde) { return pmd_val(pmde) == 0; }

static inline int pmde_bad(pmd_t pmde)
{
    return !pmde_present(pmde) || (pmd_val(pmde) & _RISCV_PG_LEAF);
}

static inline void pmde_clear(pmd_t* pmde) { *pmde = __pmd(0); }

static inline void pmde_populate(pmd_t* pmde, unsigned long pt_phys)
{
    unsigned long pfn = pt_phys >> ARCH_PG_SHIFT;
    *pmde = __pmd((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

static inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr = (pte_t*)phys_to_virt((pmd_val(*pt) >> RISCV_PG_PFN_SHIFT)
                                        << ARCH_PG_SHIFT);
    return vaddr + ARCH_PTE(addr);
}

static inline void set_pte(pte_t* ptep, pte_t pte)
{
    *(volatile pte_t*)ptep = pte;
}

static inline int pte_present(pte_t pte)
{
    return pte_val(pte) & _RISCV_PG_PRESENT;
}

#define pgd_addr_end(addr, end)                                          \
    ({                                                                   \
        vir_bytes __boundary = ((addr) + ARCH_PGD_SIZE) & ARCH_PGD_MASK; \
        (__boundary - 1 < (end)-1) ? __boundary : (end);                 \
    })

#ifndef pmd_addr_end
#define pmd_addr_end(addr, end)                                          \
    ({                                                                   \
        vir_bytes __boundary = ((addr) + ARCH_PMD_SIZE) & ARCH_PMD_MASK; \
        (__boundary - 1 < (end)-1) ? __boundary : (end);                 \
    })
#endif

#endif // _ARCH_PAGETABLE_H_
