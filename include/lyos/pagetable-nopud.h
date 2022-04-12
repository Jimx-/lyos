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

#ifndef _ARCH_PAGETABLE_NOPUD_H_
#define _ARCH_PAGETABLE_NOPUD_H_

#define __PAGETABLE_PUD_FOLDED 1

typedef struct {
    pde_t pde;
} pud_t;

#define ARCH_PUD_SHIFT      ARCH_PGD_SHIFT
#define ARCH_PUD_SIZE       (1UL << ARCH_PUD_SHIFT)
#define ARCH_PUD_MASK       (~(ARCH_PUD_SIZE - 1))
#define ARCH_VM_PUD_ENTRIES 1

static inline int pde_none(pde_t pde) { return 0; }
static inline int pde_bad(pde_t pde) { return 0; }
static inline int pde_present(pde_t pde) { return 1; }
static inline void pde_clear(pde_t* pde) {}

static inline void pde_populate(pde_t* pde, phys_bytes pud_phys) {}

static inline pud_t* pud_offset(pde_t* pud, unsigned long addr)
{
    return (pud_t*)pud;
}

#define pud_val(x) pde_val((x).pde)
#define __pud(x)   ((pud_t){__pde(x)})

#define pud_addr_end(addr, end) (end)

#endif
