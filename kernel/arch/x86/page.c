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
PUBLIC void pg_identity(pde_t * pgd)
{
    int i;
    phys_bytes phys;
    int flags = PG_PRESENT | PG_RW | PG_USER | ARCH_PG_BIGPAGE;
    /* initialize page directory */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        phys = i * ARCH_BIG_PAGE_SIZE;
        pgd[i] = phys | flags;
    }
}

PUBLIC pde_t pg_mapkernel(pde_t * pgd)
{
    phys_bytes mapped = 0, kern_phys = kinfo.kernel_start_phys;
    phys_bytes kern_len = kinfo.kernel_end_phys - kern_phys;
    int pde = ARCH_PDE(KERNEL_VMA);
    
    while (mapped < kern_len) {
        pgd[pde] = kern_phys | PG_PRESENT | PG_RW | ARCH_PG_BIGPAGE;
        mapped += ARCH_BIG_PAGE_SIZE;
        kern_phys += ARCH_BIG_PAGE_SIZE;
        pde++;
    }

    return pde;
}

PUBLIC void pg_load(pde_t * pgd)
{
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
