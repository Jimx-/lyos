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
#include <asm/page.h>
#include <asm/pagetable.h>
#include "global.h"
#include "const.h"

unsigned long va_pa_offset = 0;

static struct mmproc* mmprocess = &mmproc_table[TASK_MM];

static pud_t* alloc_pud(phys_bytes* pud_phys)
{
    pud_t* pud = (pud_t*)alloc_vmem(
        pud_phys, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES, PGT_PAGETABLE);
    if (!pud) return NULL;

    memset(pud, 0, sizeof(pud_t) * ARCH_VM_PUD_ENTRIES);
    return pud;
}

static int ident_map_pud(pud_t* pud_page, vir_bytes addr, vir_bytes end,
                         unsigned long offset, unsigned int pmd_flags,
                         int use_gbpages)
{
    vir_bytes next;

    for (; addr < end; addr = next) {
        pud_t* pude = pud_page + ARCH_PUDE(addr);

        next = (addr & ARCH_PUD_MASK) + ARCH_PUD_SIZE;
        if (next - 1 >= end) next = end;

        if (use_gbpages) {
            pud_t pud_val;

            if (pude_present(*pude)) continue;

            addr &= ARCH_PUD_MASK;
            pud_val = __pud((addr - offset) | pmd_flags);
            *pude = pud_val;
            continue;
        }
    }

    return 0;
}

static int identity_map(pde_t* pgd_page, phys_bytes pstart, phys_bytes pend,
                        unsigned long offset, unsigned long flags)
{
    vir_bytes addr = pstart + offset;
    vir_bytes end = pend + offset;
    vir_bytes next;
    unsigned long pmd_flags =
        ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_BIGPAGE | flags;
    int use_gbpages;
    int retval;

    use_gbpages = _cpufeature(_CPUF_I386_GBPAGES);

    for (; addr < end; addr = next) {
        pde_t* pgd = pgd_offset(pgd_page, addr);
        pud_t* pud;
        phys_bytes pud_phys;

        next = (addr & ARCH_PGD_MASK) + ARCH_PGD_SIZE;
        if (next - 1 >= end) next = end;

        if (pde_present(*pgd)) {
            pud = pud_offset(pgd, 0);
            retval =
                ident_map_pud(pud, addr, next, offset, pmd_flags, use_gbpages);
            if (retval) return retval;
            continue;
        }

        pud = alloc_pud(&pud_phys);
        if (!pud) return ENOMEM;

        retval = ident_map_pud(pud, addr, next, offset, pmd_flags, use_gbpages);
        if (retval) return retval;

        pde_populate(pgd, pud_phys);
    }

    return 0;
}

void arch_init_pgd(pgdir_t* pgd)
{
    int i;
    phys_bytes paddr = kernel_info.kernel_start_phys;

    va_pa_offset = KERNEL_VMA;

    identity_map(pgd->vir_addr, paddr, paddr + KERNEL_VIRT_SIZE - 1, KERNEL_VMA,
                 0);

    struct kinfo_mmap_entry* mmap;
    for (i = 0, mmap = kernel_info.memmaps; i < kernel_info.memmaps_count;
         i++, mmap++) {
        phys_bytes start = mmap->addr;
        phys_bytes end = mmap->addr + mmap->len;

        start &= ARCH_PUD_MASK;
        end = roundup(end, ARCH_PUD_SIZE);

        if (start >= end) break;

        identity_map(pgd->vir_addr, start, end, PAGE_OFFSET, ARCH_PG_USER);
    }

    va_pa_offset = PAGE_OFFSET;
}

void arch_pgd_mapkernel(pgdir_t* pgd)
{
    int i;
    pgdir_t* mypgd = &mmprocess->mm->pgd;

    for (i = ARCH_PDE(KERNEL_VMA); i < ARCH_VM_DIR_ENTRIES; i++) {
        pgd->vir_addr[i] = mypgd->vir_addr[i];
    }
}
