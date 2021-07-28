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

#include <lyos/pagetable-nopmd.h>

static inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

static inline int pmde_none(pmd_t pmde)
{
    return !(pmd_val(pmde) & ARCH_PG_PRESENT);
}

static inline int pmde_bad(pmd_t pmde)
{
    return ((pmd_val(pmde) & ~ARCH_PG_MASK) & ~ARCH_PG_USER) !=
           (ARCH_PG_PRESENT | ARCH_PG_RW);
}

static inline void pmde_clear(pmd_t* pmde) { *pmde = __pmd(0); }

static inline void pmde_populate(pmd_t* pmde, unsigned long pt_phys)
{
    *pmde = __pmd((pt_phys & I386_PG_MASK) | ARCH_PG_PRESENT | ARCH_PG_RW |
                  ARCH_PG_USER);
}

static inline int pte_none(pte_t pte)
{
    return !(pte_val(pte) & ARCH_PG_PRESENT);
}

static inline int pte_present(pte_t pte)
{
    return pte_val(pte) & ARCH_PG_PRESENT;
}

static inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr = (pte_t*)phys_to_virt(pmd_val(*pt) & ARCH_PG_MASK);
    return vaddr + ARCH_PTE(addr);
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

#endif
