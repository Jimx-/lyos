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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "page.h"
#include <errno.h>
#include "arch_proto.h"
#include <lyos/cpufeature.h>

/**
 * <Ring 0> Setup identity paging for kernel
 */
PUBLIC void setup_paging(pde_t * pgd, pte_t * pt, int kpts)
{
    pte_t * page_table_start = pt;
    /* full 4G memory */
    int nr_page_tables = 1024;

    /* identity paging */
    int nr_pages = nr_page_tables * 1024;
    int page = PG_PRESENT | PG_RW | PG_USER;

    int i;
    for (i = 0; i < nr_pages; i++, page += PG_SIZE) {
        if (i >= FIXMAP_START / PG_SIZE && i < FIXMAP_END / PG_SIZE) page_table_start[i] = 0;
        else page_table_start[i] = page;
    }

    /* initialize page directory */
    int pde = (int)page_table_start | PG_PRESENT | PG_RW | PG_USER;
    for (i = 0; i < nr_page_tables; i++, pde += PT_SIZE) {
        pgd[i] = pde;
    }

    /* map the kernel */
    for (i = 0; i < kpts; i++) {
        pgd[i] &= ~PG_USER;     /* not accessible to user */
        pgd[i + ARCH_PDE(KERNEL_VMA)] = pgd[i];
    }

    /* switch to the new page directory */
    write_cr3((u32)pgd);
    /* reload it */
    reload_cr3();
    enable_paging();
}

/* <Ring 0> */
PUBLIC void switch_address_space(struct proc * p) {
    write_cr3((u32) p->pgd.phys_addr);
    //asm volatile ("mov %0, %%cr3":: "r"(pgd));
}

PUBLIC void enable_paging()
{
    u32 cr0, cr4;
    int pge_supported;

    pge_supported = _cpufeature(_CPUF_I386_PGE);

    cr0 = read_cr0();
    cr4 = read_cr4();

    write_cr4(cr4 & ~(I386_CR4_PGE | I386_CR4_PSE));
    cr4 = read_cr4();

    /* enable PSE */
    cr4 |= I386_CR4_PSE;
    write_cr4(cr4);

    cr0 |= I386_CR0_PG;
    write_cr0(cr0);
    cr0 |= I386_CR0_WP;
    write_cr0(cr0);

    if (pge_supported) cr4 |= I386_CR4_PGE;

    write_cr4(cr4);
}

/* <Ring 0> */
PUBLIC void disable_paging()
{
    int cr0;
    cr0 = read_cr0();
    cr0 &= ~I386_CR0_PG;
    write_cr0(cr0);
}
