#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#include <asm/barrier.h>
#include <asm/page.h>

#if CONFIG_PGTABLE_LEVELS == 2
#include <asm-generic/pagetable-nopmd.h>
#elif CONFIG_PGTABLE_LEVELS == 3
#include <asm-generic/pagetable-nopud.h>
#endif

#define _ARM64_PTE_WRITE (_ARM64_PTE_DBM)

#define _ARM64_PG_BASE \
    (_ARM64_PTE_TYPE_PAGE | _ARM64_PTE_AF | _ARM64_PTE_SHARED)
#define _ARM64_SECT_BASE (_ARM64_PMD_TYPE_SECT | _ARM64_SECT_AF | _ARM64_SECT_S)

#define _ARM64_DEVICE_nGnRnE                                               \
    (_ARM64_PG_BASE | _ARM64_PTE_PXN | _ARM64_PTE_UXN | _ARM64_PTE_WRITE | \
     _ARM64_PTE_ATTRINDX(MT_DEVICE_nGnRnE))
#define _ARM64_DEVICE_nGnRE                                                \
    (_ARM64_PG_BASE | _ARM64_PTE_PXN | _ARM64_PTE_UXN | _ARM64_PTE_WRITE | \
     _ARM64_PTE_ATTRINDX(MT_DEVICE_nGnRE))
#define _ARM64_PG_NORMAL                                                   \
    (_ARM64_PG_BASE | _ARM64_PTE_PXN | _ARM64_PTE_UXN | _ARM64_PTE_WRITE | \
     _ARM64_PTE_ATTRINDX(MT_NORMAL))

#define ARM64_PG_KERNEL __pgprot(_ARM64_PG_NORMAL)

#define ARM64_PG_NONE                                                     \
    __pgprot(((_ARM64_PG_BASE) & ~_ARM64_PTE_VALID) | _ARM64_PTE_RDONLY | \
             _ARM64_PTE_NG | _ARM64_PTE_PXN | _ARM64_PTE_UXN)

#define ARM64_PG_SHARED                                         \
    __pgprot(_ARM64_PG_BASE | _ARM64_PTE_USER | _ARM64_PTE_NG | \
             _ARM64_PTE_PXN | _ARM64_PTE_UXN | _ARM64_PTE_WRITE)
#define ARM64_PG_SHARED_EXEC                                    \
    __pgprot(_ARM64_PG_BASE | _ARM64_PTE_USER | _ARM64_PTE_NG | \
             _ARM64_PTE_PXN | _ARM64_PTE_WRITE)
#define ARM64_PG_READONLY                                           \
    __pgprot(_ARM64_PG_BASE | _ARM64_PTE_USER | _ARM64_PTE_RDONLY | \
             _ARM64_PTE_NG | _ARM64_PTE_PXN | _ARM64_PTE_UXN)
#define ARM64_PG_READONLY_EXEC                                      \
    __pgprot(_ARM64_PG_BASE | _ARM64_PTE_USER | _ARM64_PTE_RDONLY | \
             _ARM64_PTE_NG | _ARM64_PTE_PXN)
#define ARM64_PG_EXECONLY                                         \
    __pgprot(_ARM64_PG_BASE | _ARM64_PTE_RDONLY | _ARM64_PTE_NG | \
             _ARM64_PTE_PXN)

/*         xwr */
#define __P000 ARM64_PG_NONE
#define __P001 ARM64_PG_READONLY
#define __P010 ARM64_PG_SHARED
#define __P011 ARM64_PG_SHARED
#define __P100 ARM64_PG_READONLY_EXEC
#define __P101 ARM64_PG_READONLY_EXEC
#define __P110 ARM64_PG_SHARED_EXEC
#define __P111 ARM64_PG_SHARED_EXEC

#define __pgprot_modify(prot, mask, bits) \
    __pgprot((pgprot_val(prot) & ~(mask)) | (bits))

#define pgprot_noncached(prot)                                               \
    __pgprot_modify(prot, _ARM64_PTE_ATTRINDX_MASK,                          \
                    _ARM64_PTE_ATTRINDX(MT_DEVICE_nGnRnE) | _ARM64_PTE_PXN | \
                        _ARM64_PTE_UXN)

static inline pmd_t pude_pmd(pud_t pude) { return __pmd(pud_val(pude)); }
static inline pte_t pude_pte(pud_t pude) { return __pte(pud_val(pude)); }

static inline pte_t pmde_pte(pmd_t pmde) { return __pte(pmd_val(pmde)); }

static inline pgprot_t mk_pud_sect_prot(pgprot_t prot)
{
    return __pgprot((pgprot_val(prot) & ~_ARM64_PUD_TABLE_BIT) |
                    _ARM64_PUD_TYPE_SECT);
}

static inline pgprot_t mk_pmd_sect_prot(pgprot_t prot)
{
    return __pgprot((pgprot_val(prot) & ~_ARM64_PMD_TABLE_BIT) |
                    _ARM64_PMD_TYPE_SECT);
}

#define __pte_to_phys(pte) (pte_val(pte) & _ARM64_PTE_ADDR_MASK)

#define pte_pfn(pte) (__pte_to_phys(pte) >> ARCH_PG_SHIFT)
#define pfn_pte(pfn, prot) \
    __pte((pteval_t)((phys_bytes)(pfn) << ARCH_PG_SHIFT) | pgprot_val(prot))

#define pfn_pmd(pfn, prot) \
    __pmd((pmdval_t)((phys_bytes)(pfn) << ARCH_PG_SHIFT) | pgprot_val(prot))

#define pfn_pud(pfn, prot) \
    __pud((pudval_t)((phys_bytes)(pfn) << ARCH_PG_SHIFT) | pgprot_val(prot))

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

#define pte_present(pte) (!!(pte_val(pte) & _ARM64_PTE_VALID))

static inline void set_pte(pte_t* ptep, pte_t pte)
{
    *(volatile pte_t*)ptep = pte;

    dsb(ishst);
    isb();
}

static inline void pte_clear(pte_t* pte) { set_pte(pte, __pte(0)); }

#define pmde_none(x) (!pmd_val(x))
#define pmde_table(pmde) \
    ((pmd_val(pmde) & _ARM64_PMD_TYPE_MASK) == _ARM64_PMD_TYPE_TABLE)
#define pmde_sect(pmde) \
    ((pmd_val(pmde) & _ARM64_PMD_TYPE_MASK) == _ARM64_PMD_TYPE_SECT)
#define pmde_bad(pmde) (!pmde_table(pmde))

static inline void __pmde_populate(pmd_t* pmde, phys_bytes pte_phys,
                                   pmdval_t prot)
{
    *pmde = __pmd(pte_phys | prot);
}

static inline void pmde_populate(pmd_t* pmde, phys_bytes pte_phys)
{
    __pmde_populate(pmde, pte_phys, _ARM64_PMD_TYPE_TABLE);
}

static inline void set_pmde(pmd_t* pmdep, pmd_t pmde)
{
    *(volatile pmd_t*)pmdep = pmde;

    dsb(ishst);
    isb();
}

static inline void pmde_clear(pmd_t* pmde) { set_pmde(pmde, __pmd(0)); }

static inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr = (pte_t*)phys_to_virt(pmde_page_paddr(*pt));
    return vaddr + ARCH_PTE(addr);
}

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

static inline void pude_populate(pud_t* pude, phys_bytes pmd_phys)
{
    __pude_populate(pude, pmd_phys, _ARM64_PUD_TYPE_TABLE);
}

#endif

#if defined(CONFIG_ARM64_64K_PAGES) || CONFIG_PGTABLE_LEVELS < 3
static inline bool pude_sect(pud_t pude) { return false; }
static inline bool pude_table(pud_t pude) { return true; }
#else
#define pude_sect(pude) \
    ((pud_val(pude) & _ARM64_PUD_TYPE_MASK) == _ARM64_PUD_TYPE_SECT)
#define pude_table(pude) \
    ((pud_val(pude) & _ARM64_PUD_TYPE_MASK) == _ARM64_PUD_TYPE_TABLE)
#endif

#define pude_none(x) (!pud_val(x))
#define pude_bad(x)  (!pude_table(x))

static inline void set_pude(pud_t* pudep, pud_t pude)
{
    *(volatile pud_t*)pudep = pude;

    dsb(ishst);
    isb();
}

static inline void pude_clear(pud_t* pude) { set_pude(pude, __pud(0)); }

#if CONFIG_PGTABLE_LEVELS > 3

static inline void __pde_populate(pde_t* pde, phys_bytes pud_phys,
                                  pdeval_t prot)
{
    *pde = __pde(pud_phys | prot);
}

static inline void pde_populate(pde_t* pde, phys_bytes pud_phys)
{
    __pde_populate(pde, pud_phys, _ARM64_PGD_TYPE_TABLE);
}

#else

static inline pmd_t* pmd_offset(pud_t* pmd, unsigned long addr)
{
    pmd_t* vaddr = (pmd_t*)phys_to_virt(pude_page_paddr(*pmd));
    return vaddr + ARCH_PMDE(addr);
}

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

static inline int pud_sect_supported(void) { return ARCH_PG_SIZE == 0x1000; }

#endif
