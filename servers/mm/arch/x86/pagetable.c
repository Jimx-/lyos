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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/vm.h>
#include "region.h"
#include "proto.h"
#include <lyos/cpufeature.h>
#include <asm/pagetable.h>
#include "global.h"
#include "const.h"

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];

void arch_create_kern_mapping(phys_bytes phys_addr, vir_bytes vir_addr,
                              size_t len, int flags)
{
    pgdir_t* mypgd = &mmprocess->mm->pgd;
    unsigned long page_prot;

    page_prot = ARCH_PG_PRESENT;
    if (flags & KMF_USER) page_prot |= ARCH_PG_USER;

    if (flags & KMF_WRITE) page_prot |= ARCH_PG_RW;

    pt_writemap(mypgd, phys_addr, vir_addr, len, __pgprot(page_prot));
}

void arch_pgd_new(pgdir_t* pgd)
{
    pgdir_t* mypgd = &mmprocess->mm->pgd;

    memcpy(&pgd->vir_addr[ARCH_PDE(KERNEL_VMA)],
           &mypgd->vir_addr[ARCH_PDE(KERNEL_VMA)],
           (ARCH_VM_DIR_ENTRIES - ARCH_PDE(KERNEL_VMA)) * sizeof(pde_t));
}
