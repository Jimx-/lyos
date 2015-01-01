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
#include "arch_const.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
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
        page_table_start[i] = page;
    }

    phys_bytes phys;
    int flags = PG_PRESENT | PG_RW | PG_USER | ARCH_PG_BIGPAGE;
    /* initialize page directory */
    int pde = (int)page_table_start | PG_PRESENT | PG_RW | PG_USER;
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++, pde += PT_SIZE) {
        phys = i * ARCH_BIG_PAGE_SIZE;
        pgd[i] = phys | flags;
    }

    /* map the kernel */
    for (i = 0; i < kpts; i++) {
        pgd[i] &= ~PG_USER;     /* not accessible to user */
        pgd[i + ARCH_PDE(KERNEL_VMA)] = pgd[i];
    }

    /* switch to the new page directory */
    write_cr3((u32)pgd);
    enable_paging();
}

/* <Ring 0> */
PUBLIC void switch_address_space(struct proc * p) {
    get_cpulocal_var(pt_proc) = p;
    write_cr3(p->seg.cr3_phys);
}

PUBLIC void enable_paging()
{
    u32 cr4;
    int pge_supported;

    pge_supported = _cpufeature(_CPUF_I386_PGE);

    if (pge_supported) {
        cr4 = read_cr4();
        cr4 |= I386_CR4_PGE;
        write_cr4(cr4);
    }
}

/* <Ring 0> */
PUBLIC void disable_paging()
{
    int cr0;
    cr0 = read_cr0();
    cr0 &= ~I386_CR0_PG;
    write_cr0(cr0);
}
