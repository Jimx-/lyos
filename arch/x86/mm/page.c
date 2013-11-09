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
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"

PRIVATE int map_kernel(struct page_directory * pgd);

/**
 * <Ring 0> Setup identity paging for kernel
 */
PUBLIC void setup_paging(unsigned int memory_size, pde_t * pgd, pte_t * pt)
{
    pte_t * page_table_start = pt;
    /* full 4G memory */
    int nr_page_tables = 1024;

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

    /* switch to the new page directory */
    switch_address_space(pgd);
    /* reload it */
    reload_cr3();
}

/* <Ring 0> */
PUBLIC void switch_address_space(pde_t * pgd) {
    asm volatile ("mov %0, %%cr3":: "r"(pgd));
}

/* <Ring 0> */
PUBLIC void disable_paging()
{
    int cr0;
    asm volatile ("mov %%cr0, %0": "=r"(cr0));
    cr0 &= ~I386_CR0_PG;
    asm volatile ("mov %0, %%cr0":: "r"(cr0));
}

/* <Ring 1> */
PUBLIC int pgd_new(struct page_directory * pgd)
{
    /* map the directory so that we can write it */
    pde_t * pg_dir = (pde_t *)alloc_vmem(PGD_SIZE);
    pgd->phys_addr = va2pa(getpid(), pg_dir);
    pgd->vir_addr = pg_dir;

    int i;

    /* zero it */
    for (i = 0; i < I386_VM_DIR_ENTRIES; i++) {
        pg_dir[i] = 0;
    }

    for (i = 0; i < I386_VM_DIR_ENTRIES; i++) {
        pgd->vir_pts[i] = NULL;
    }

    map_kernel(pg_dir);
    return 0;
}

/**
 * <Ring 1> Map the kernel.
 * @param  pgd The page directory.
 * @return     Zero on success.
 */
PRIVATE int map_kernel(struct page_directory * pgd)
{
    int i;

    for (i = KERNEL_VMA / 0x400000; i < KERNEL_VMA / 0x400000 + 4; i++) {
        pgd->vir_addr[KERNEL_VMA / 0x400000] = initial_pgd[i - KERNEL_VMA / 0x400000];
        pgd->vir_pts[i] = (initial_pgd[i - KERNEL_VMA / 0x400000] + KERNEL_VMA) & 0xfffff000;
    }
    return 0;
}
