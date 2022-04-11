#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#if CONFIG_PGTABLE_LEVELS == 2
#include <lyos/pagetable-nopmd.h>
#elif CONFIG_PGTABLE_LEVELS == 3
#include <lyos/pagetable-nopud.h>
#endif

#define _ARM64_PTE_WRITE (_ARM64_PTE_DBM)

#define _ARM64_PG_BASE \
    (_ARM64_PTE_TYPE_PAGE | _ARM64_PTE_AF | _ARM64_PTE_SHARED)
#define _ARM64_SECT_BASE (_ARM64_PMD_TYPE_SECT | _ARM64_SECT_AF | _ARM64_SECT_S)

#define _ARM64_PG_NORMAL                                                   \
    (_ARM64_PG_BASE | _ARM64_PTE_PXN | _ARM64_PTE_UXN | _ARM64_PTE_WRITE | \
     _ARM64_PTE_ATTRINDX(MT_NORMAL))

#define ARM64_PG_KERNEL __pgprot(_ARM64_PG_NORMAL)

static inline pmd_t pude_pmd(pud_t pude) { return __pmd(pud_val(pude)); }
static inline pte_t pude_pte(pud_t pude) { return __pte(pud_val(pude)); }

static inline pte_t pmde_pte(pmd_t pmde) { return __pte(pmd_val(pmde)); }

static inline pgprot_t mk_pmd_sect_prot(pgprot_t prot)
{
    return __pgprot((pgprot_val(prot) & ~_ARM64_PMD_TABLE_BIT) |
                    _ARM64_PMD_TYPE_SECT);
}

#define pfn_pte(pfn, prot) \
    __pte((pteval_t)((phys_bytes)(pfn) << ARCH_PG_SHIFT) | pgprot_val(prot))

#define pfn_pmd(pfn, prot) \
    __pmd((pmdval_t)((phys_bytes)(pfn) << ARCH_PG_SHIFT) | pgprot_val(prot))

#define __pte_to_phys(pte) (pte_val(pte) & _ARM64_PTE_ADDR_MASK)

static inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

static inline phys_bytes pmde_page_paddr(pmd_t pmde)
{
    return __pte_to_phys(pmde_pte(pmde));
}

#define pte_offset_phys(dir, addr) \
    (pmde_page_paddr(*(dir)) + ARCH_PTE(addr) * sizeof(pte_t))

#define pte_set_fixmap(addr) ((pte_t*)set_fixmap_offset(FIX_PTE, addr))
#define pte_set_fixmap_offset(pmd, addr) \
    pte_set_fixmap(pte_offset_phys(pmd, addr))
#define pte_clear_fixmap() clear_fixmap(FIX_PTE)

static inline void __pmde_populate(pmd_t* pmde, phys_bytes pte_phys,
                                   pmdval_t prot)
{
    *pmde = __pmd(pte_phys | prot);
}

#define pmde_none(x) (!pmd_val(x))

#if CONFIG_PGTABLE_LEVELS > 2

static inline phys_bytes pude_page_paddr(pud_t pude)
{
    return __pte_to_phys(pude_pte(pude));
}

#define pmd_offset_phys(dir, addr) \
    (pude_page_paddr(*(dir)) + ARCH_PMDE(addr) * sizeof(pmd_t))

#define pmd_set_fixmap(addr) ((pmd_t*)set_fixmap_offset(FIX_PMD, addr))
#define pmd_set_fixmap_offset(pud, addr) \
    pmd_set_fixmap(pmd_offset_phys(pud, addr))
#define pmd_clear_fixmap() clear_fixmap(FIX_PMD)

#define pmd_offset_kimg(dir, addr) \
    ((pmd_t*)__phys_to_kimg(pmd_offset_phys((dir), (addr))))

static inline void __pude_populate(pud_t* pude, phys_bytes pmd_phys,
                                   pudval_t prot)
{
    *pude = __pud(pmd_phys | prot);
}
#endif

#define pude_none(x) (!pud_val(x))

#if CONFIG_PGTABLE_LEVELS > 3

static inline void __pde_populate(pde_t* pde, phys_bytes pud_phys,
                                  pdeval_t prot)
{
    *pde = __pde(pud_phys | prot);
}

#else

#define pud_set_fixmap(addr)             NULL
#define pud_set_fixmap_offset(dir, addr) ((pud_t*)dir)
#define pud_clear_fixmap()

#define pud_offset_kimg(dir, addr) ((pud_t*)dir)

static inline void __pde_populate(pde_t* pde, phys_bytes pud_phys,
                                  pdeval_t prot)
{}

#endif

#define pgd_addr_end(addr, end)                                          \
    ({                                                                   \
        vir_bytes __boundary = ((addr) + ARCH_PGD_SIZE) & ARCH_PGD_MASK; \
        (__boundary - 1 < (end)-1) ? __boundary : (end);                 \
    })

#ifndef pud_addr_end
#define pud_addr_end(addr, end)                                          \
    ({                                                                   \
        vir_bytes __boundary = ((addr) + ARCH_PUD_SIZE) & ARCH_PUD_MASK; \
        (__boundary - 1 < (end)-1) ? __boundary : (end);                 \
    })
#endif

#ifndef pmd_addr_end
#define pmd_addr_end(addr, end)                                          \
    ({                                                                   \
        vir_bytes __boundary = ((addr) + ARCH_PMD_SIZE) & ARCH_PMD_MASK; \
        (__boundary - 1 < (end)-1) ? __boundary : (end);                 \
    })
#endif

#endif
