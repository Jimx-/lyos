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

#ifndef _MM_PAGETABLE_H_
#define _MM_PAGETABLE_H_

#define pgd_addr_end(addr, end)						\
({	vir_bytes __boundary = ((addr) + ARCH_PGD_SIZE) & ARCH_PGD_MASK;	\
    (__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

#ifndef pmd_addr_end
#define pmd_addr_end(addr, end)						\
({	vir_bytes __boundary = ((addr) + ARCH_PMD_SIZE) & ARCH_PMD_MASK;	\
    (__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

#endif

#endif
