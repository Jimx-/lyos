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

#define __PAGETABLE_PMD_FOLDED 1

typedef struct {
    pde_t pde;
} pmd_t;

#define ARCH_VM_PMD_ENTRIES 1

PRIVATE inline int pde_none(pde_t pde) { return 0; }
PRIVATE inline int pde_bad(pde_t pde) { return 0; }
PRIVATE inline int pde_present(pde_t pde) { return 1; }
PRIVATE inline void pde_clear(pde_t* pde) {}

PRIVATE inline pmd_t* pmd_offset(pde_t* pmd, unsigned long addr)
{
    return (pmd_t*)pmd;
}

#define pmd_val(x) pde_val((x).pde)
#define __pmd(x) ((pmd_t){__pde(x)})

#define pmd_addr_end(addr, end) (end)

#endif
