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

#ifndef _PAGE_H_
#define _PAGE_H_

#define KERNEL_VMA  0xF0000000

typedef unsigned int    pde_t;
typedef unsigned int    pte_t;

#define PGD_SIZE    0x1000
#define PT_SIZE     0x1000
#define PG_SIZE     0x1000

#define PG_PRESENT  0x001 
#define PG_RW       0x002
#define PG_USER     0x004

#define I386_CR0_PG 0x80000000

PUBLIC void setup_paging(unsigned int memory_size, pde_t * pgd);
PUBLIC void switch_address_space(pde_t * pgd);
PUBLIC void enable_paging();
PUBLIC void disable_paging();

#endif
