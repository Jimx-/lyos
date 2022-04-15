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

static int global_bit = 0;

unsigned long va_pa_offset = 0;

void arch_init_pgd(pgdir_t* pgd)
{
    int kernel_pde = kernel_info.kernel_start_pde;
    phys_bytes paddr = kernel_info.kernel_start_phys;

    if (_cpufeature(_CPUF_I386_PGE)) global_bit = ARCH_PG_GLOBAL;

    /* map kernel for MM */
    while (kernel_pde < ARCH_PDE(KERNEL_VMA + LOWMEM_END)) {
        pgd->vir_addr[kernel_pde] =
            __pde(paddr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW);

        paddr += ARCH_BIG_PAGE_SIZE;
        kernel_pde++;
    }

    /* create direct mapping to access physical memory */
    for (kernel_pde = 0, paddr = 0; kernel_pde < ARCH_PDE(LOWMEM_END);
         kernel_pde++, paddr += ARCH_BIG_PAGE_SIZE) {
        if (paddr < kernel_info.kernel_end_phys) {
            continue;
        }

        pgd->vir_addr[kernel_pde] =
            __pde(paddr | ARCH_PG_PRESENT | ARCH_PG_BIGPAGE | ARCH_PG_RW |
                  ARCH_PG_USER);
    }
}
