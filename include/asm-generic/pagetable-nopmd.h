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

#ifndef _ARCH_PAGETABLE_NOPMD_H_
#define _ARCH_PAGETABLE_NOPMD_H_

#include <asm-generic/pagetable-nopud.h>

#define __PAGETABLE_PMD_FOLDED 1

typedef struct {
    pud_t pud;
} pmd_t;

#define ARCH_VM_PMD_ENTRIES 1

static inline int pude_none(pud_t pude) { return 0; }
static inline int pude_bad(pud_t pude) { return 0; }
static inline int pude_present(pud_t pude) { return 1; }
static inline void pude_clear(pud_t* pude) {}

static inline void pude_populate(pud_t* pud, phys_bytes pmd_phys) {}

static inline pmd_t* pmd_offset(pud_t* pmd, unsigned long addr)
{
    return (pmd_t*)pmd;
}

#define pmd_val(x) pud_val((x).pud)
#define __pmd(x)   ((pmd_t){__pud(x)})

#define pmd_addr_end(addr, end) (end)

#endif
