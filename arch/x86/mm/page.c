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

#include "lyos/type.h"
#include "page.h"

PUBLIC void setup_paging(unsigned int memory_size, pde_t * pgd, pte_t * pt)
{
    pte_t * page_table_start = pt;
    int nr_page_tables = memory_size / 0x400000 + 1;

    /* identity paging */
    int nr_pages = nr_page_tables * 1024;
    int page = PG_PRESENT | PG_RW | PG_USER;

    int i;
    for (i = 0; i < nr_pages; i++, page += PG_SIZE) {
        page_table_start[i] = page;
    }

    /* initialize page directory */
    int pde = (int)page_table_start | PG_PRESENT | PG_RW | PG_USER;
    for (i = 0; i < nr_page_tables; i++, pde += PT_SIZE) {
        pgd[i] = pde;
    }

    /* map 0xF0000000 ~ 0xF1000000 to 0x00000000 ~ 0x01000000 */
    for (i = KERNEL_VMA / 0x400000; i < KERNEL_VMA / 0x400000 + 4; i++) {
        pgd[i] = pgd[i - KERNEL_VMA / 0x400000];
    }

    /* unmap first 16M */
    //for (i = 0; i < 4; i++) {
    //    pgd[i] = 0;
    //}

    /* switch to the new page directory */
    switch_address_space(pgd);
    /* reload it */
    reload_cr3();
}

PUBLIC void switch_address_space(pde_t * pgd) {
    asm volatile ("mov %0, %%cr3":: "r"(pgd));
}

PUBLIC void disable_paging()
{
    int cr0;
    asm volatile ("mov %%cr0, %0": "=r"(cr0));
    cr0 &= ~I386_CR0_PG;
    asm volatile ("mov %0, %%cr0":: "r"(cr0));
}
