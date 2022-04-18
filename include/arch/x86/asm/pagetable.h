/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ARCH_PAGETABLE_H_
#define _ARCH_PAGETABLE_H_

#include <asm/page.h>

#if CONFIG_PGTABLE_LEVELS < 3
#include <asm-generic/pagetable-nopmd.h>
#endif

static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
    return __pte((pfn << ARCH_PG_SHIFT) | pgprot_val(prot));
}

static inline unsigned long pte_pfn(pte_t pte)
{
    return pte_val(pte) >> ARCH_PG_SHIFT;
}

static inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

#if CONFIG_PGTABLE_LEVELS > 3
static inline int pde_present(pde_t pde)
{
    return pde_val(pde) & ARCH_PG_PRESENT;
}

static inline int pde_none(pde_t pde) { return pde_val(pde) == 0; }

static inline int pde_bad(pde_t pde)
{
    return ((pde_val(pde) & ~ARCH_PG_MASK) & ~ARCH_PG_USER) !=
           _I386_PG_KERN_TABLE;
}

static inline void pde_clear(pde_t* pde) { *pde = __pde(0); }

static inline void pde_populate(pde_t* pde, unsigned long pud_phys)
{
    *pde = __pde((pud_phys & ARCH_PG_MASK) | _I386_PG_TABLE);
}

static inline pud_t* pud_offset(pde_t* pud, unsigned long addr)
{
    pud_t* vaddr = (pud_t*)phys_to_virt(pde_val(*pud) & ARCH_PG_MASK);
    return vaddr + ARCH_PUDE(addr);
}
#endif

static inline int pde_large(pde_t pde) { return 0; }

#if CONFIG_PGTABLE_LEVELS > 2
static inline int pude_present(pud_t pude)
{
    return pud_val(pude) & ARCH_PG_PRESENT;
}

static inline int pude_none(pud_t pude) { return pud_val(pude) == 0; }

static inline int pude_bad(pud_t pude)
{
    return ((pud_val(pude) & ~ARCH_PG_MASK) & ~ARCH_PG_USER) !=
           _I386_PG_KERN_TABLE;
}

static inline int pude_large(pud_t pude)
{
    return (pud_val(pude) & (ARCH_PG_PRESENT | ARCH_PG_BIGPAGE)) ==
           (ARCH_PG_PRESENT | ARCH_PG_BIGPAGE);
}

static inline void pude_clear(pud_t* pude) { *pude = __pud(0); }

static inline void pude_populate(pud_t* pude, unsigned long pmd_phys)
{
    *pude = __pud((pmd_phys & ARCH_PG_MASK) | _I386_PG_TABLE);
}

static inline pmd_t* pmd_offset(pud_t* pmd, unsigned long addr)
{
    pmd_t* vaddr = (pmd_t*)phys_to_virt(pud_val(*pmd) & ARCH_PG_MASK);
    return vaddr + ARCH_PMDE(addr);
}
#else
static inline int pude_large(pud_t pude) { return 0; }
#endif

static inline int pmde_present(pmd_t pmde)
{
    return !!(pmd_val(pmde) & ARCH_PG_PRESENT);
}

static inline int pmde_large(pmd_t pmde)
{
    return (pmd_val(pmde) & (ARCH_PG_PRESENT | ARCH_PG_BIGPAGE)) ==
           (ARCH_PG_PRESENT | ARCH_PG_BIGPAGE);
}

static inline int pmde_none(pmd_t pmde) { return pmd_val(pmde) == 0; }

static inline int pmde_bad(pmd_t pmde)
{
    return ((pmd_val(pmde) & ~ARCH_PG_MASK) & ~ARCH_PG_USER) !=
           _I386_PG_KERN_TABLE;
}

static inline void pmde_clear(pmd_t* pmde) { *pmde = __pmd(0); }

static inline void pmde_populate(pmd_t* pmde, unsigned long pt_phys)
{
    *pmde = __pmd((pt_phys & ARCH_PG_MASK) | _I386_PG_TABLE);
}

static inline int pte_none(pte_t pte) { return pte_val(pte) == 0; }

static inline int pte_present(pte_t pte)
{
    return pte_val(pte) & ARCH_PG_PRESENT;
}

static inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr = (pte_t*)phys_to_virt(pmd_val(*pt) & ARCH_PG_MASK);
    return vaddr + ARCH_PTE(addr);
}

static inline void set_pte(pte_t* ptep, pte_t pte)
{
    *(volatile pte_t*)ptep = pte;
}

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
