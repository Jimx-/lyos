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

#ifndef __PAGETABLE_PMD_FOLDED
#define NUM_INIT_PMDS (((uintptr_t)-KERNEL_VMA >> ARCH_PGD_SHIFT) - 2)
pmd_t initial_pmd[ARCH_VM_PMD_ENTRIES * NUM_INIT_PMDS]
    __attribute__((aligned(ARCH_PG_SIZE)));
#endif

unsigned long va_pa_offset;

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];

void arch_init_pgd(pgdir_t* pgd)
{
    int i;
    phys_bytes pa;
    pgprot_t prot = __pgprot(pgprot_val(RISCV_PG_KERNEL) | _RISCV_PG_EXEC);

    pa = kernel_info.kernel_start_phys;

    /* Create kernel mapping. */
#ifndef __PAGETABLE_PMD_FOLDED
    for (i = 0; i < NUM_INIT_PMDS; i++) {
        phys_bytes pmd_phys;

        if (umap(SELF, UMT_VADDR,
                 (vir_bytes)&initial_pmd[ARCH_VM_PMD_ENTRIES * i],
                 sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES, &pmd_phys))
            panic("MM: can't get phys addr for bootstrap PMD page");

        pgd->vir_addr[(KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES +
                      i] =
            pfn_pde((pmd_phys >> ARCH_PG_SHIFT) + i, __pgprot(_RISCV_PG_TABLE));
    }

    for (i = 0; i < sizeof(initial_pmd) / sizeof(pmd_t); i++) {
        initial_pmd[i] =
            pfn_pmd((pa + i * ARCH_PMD_SIZE) >> ARCH_PG_SHIFT, prot);
    }
#endif

    /* Create linear mapping. */
    pa = pa & ARCH_PGD_MASK;
    va_pa_offset = PAGE_OFFSET - pa;

    /* Map all memory banks. */
    struct kinfo_mmap_entry* mmap;
    for (i = 0, mmap = kernel_info.memmaps; i < kernel_info.memmaps_count;
         i++, mmap++) {
        phys_bytes start = mmap->addr;
        phys_bytes end = mmap->addr + mmap->len;

        start &= ARCH_PGD_MASK;

        if (start >= end) break;

        if (start <= __pa(PAGE_OFFSET) && __pa(PAGE_OFFSET) < end)
            start = __pa(PAGE_OFFSET);

        for (pa = start; pa < end; pa += ARCH_PGD_SIZE) {
            uintptr_t va = (uintptr_t)__va(pa);

            pgd->vir_addr[ARCH_PDE(va)] =
                pfn_pde(pa >> ARCH_PG_SHIFT, RISCV_PG_WRITE);
        }
    }
}

void arch_pgd_mapkernel(pgdir_t* pgd)
{
    int i;
    pgdir_t* mypgd = &mmprocess->mm->pgd;

#ifndef __PAGETABLE_PMD_FOLDED
    for (i = 0; i < NUM_INIT_PMDS; i++) {
        unsigned long idx =
            (KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES + i;
        pgd->vir_addr[idx] = mypgd->vir_addr[idx];
    }
#endif
}
