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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/page.h>
#include <errno.h>
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/cpufeature.h>

PUBLIC void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end)
{
    int i;

    for (i = 0; i < pk->memmaps_count; i++) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];
        u64 mmap_end = entry->addr + entry->len;
        if (start >= entry->addr && end <= mmap_end) {
            entry->addr = end;
            entry->len = mmap_end - entry->addr;
        }
    }
}

PUBLIC phys_bytes pg_alloc_page(kinfo_t* pk)
{
    int i;

    for (i = pk->memmaps_count - 1; i >= 0; i--) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];

        if (entry->type != KINFO_MEMORY_AVAILABLE) continue;

        if (!(entry->addr % ARCH_PG_SIZE) && (entry->len >= ARCH_PG_SIZE)) {
            entry->addr += ARCH_PG_SIZE;
            entry->len -= ARCH_PG_SIZE;

            return entry->addr - ARCH_PG_SIZE;
        }
    }

    return 0;
}

PUBLIC phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size)
{
    int i;

    for (i = 0; i < pk->memmaps_count; i++) {
        struct kinfo_mmap_entry* entry = &pk->memmaps[i];

        if (entry->type != KINFO_MEMORY_AVAILABLE) continue;

        if (!(entry->addr % ARCH_PG_SIZE) && (entry->len >= size)) {
            entry->addr += size;
            entry->len -= size;

            return entry->addr - size;
        }
    }

    return 0;
}

PRIVATE pte_t* pg_alloc_pt(phys_bytes* ph)
{
    pte_t* ret;
#define PG_PAGETABLES 6
    static pte_t pagetables[PG_PAGETABLES][1024]
        __attribute__((aligned(ARCH_PG_SIZE)));
    static int pt_inuse = 0;

    if (pt_inuse >= PG_PAGETABLES) panic("no more pagetables");

    ret = pagetables[pt_inuse++];
    *ph = (phys_bytes)ret - KERNEL_VMA;

    return ret;
}

PUBLIC void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end,
                   kinfo_t* pk)
{
    pte_t* pt;
    pde_t* pgd = initial_pgd;
    if (phys_addr % ARCH_PG_SIZE)
        phys_addr = (phys_addr / ARCH_PG_SIZE) * ARCH_PG_SIZE;
    if ((uintptr_t)vir_addr % ARCH_PG_SIZE)
        vir_addr = (void*)(((uintptr_t)vir_addr / ARCH_PG_SIZE) * ARCH_PG_SIZE);

    while (vir_addr < vir_end) {
        phys_bytes phys = phys_addr;
        if (phys == 0) phys = pg_alloc_page(pk);

        int pde = ARCH_PDE(vir_addr);
        int pte = ARCH_PTE(vir_addr);

        if (!pde_val(pgd[pde]) || pde_val(pgd[pde]) & ARCH_PG_BIGPAGE) {
            phys_bytes pt_ph;
            pt = pg_alloc_pt(&pt_ph);
            pgd[pde] = __pde((pt_ph & ARCH_PG_MASK) | ARCH_PG_PRESENT |
                             ARCH_PG_RW | ARCH_PG_USER);
        } else {
            pt = (pte_t*)__va(pde_val(pgd[pde]) & ARCH_PG_MASK);
        }

        pt[pte] = __pte((phys & ARCH_PG_MASK) | ARCH_PG_PRESENT | ARCH_PG_RW |
                        ARCH_PG_USER);
        vir_addr += ARCH_PG_SIZE;
        if (phys_addr != 0) phys_addr += ARCH_PG_SIZE;
    }
}

/**
 * <Ring 0> Setup identity paging for kernel
 */
PUBLIC void pg_identity(pde_t* pgd)
{
    int i;
    phys_bytes phys;
    int flags = ARCH_PG_PRESENT | ARCH_PG_RW | ARCH_PG_USER | ARCH_PG_BIGPAGE;
    /* initialize page directory */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        if (i >= kinfo.kernel_start_pde && i < kinfo.kernel_end_pde)
            continue; /* don't touch kernel */
        phys = i * ARCH_BIG_PAGE_SIZE;
        pgd[i] = __pde(phys | flags);
    }
}

PUBLIC void pg_mapkernel(pde_t* pgd)
{
    phys_bytes mapped = 0, kern_phys = kinfo.kernel_start_phys;
    /* phys_bytes kern_len = kinfo.kernel_end_phys - kern_phys; */
    phys_bytes kern_len = LOWMEM_END;
    unsigned long pde = ARCH_PDE(KERNEL_VMA);
    int user_flag = 0;

    while (mapped < kern_len) {
        if (kern_phys >= kinfo.kernel_end_phys) {
            user_flag = ARCH_PG_USER;
        }
        pgd[pde] = __pde(kern_phys | ARCH_PG_PRESENT | ARCH_PG_RW |
                         ARCH_PG_BIGPAGE | user_flag);
        mapped += ARCH_BIG_PAGE_SIZE;
        kern_phys += ARCH_BIG_PAGE_SIZE;
        pde++;
    }
}

PUBLIC void pg_load(pde_t* pgd)
{
    write_cr3(__pa(pgd));
    enable_paging();
}

/* <Ring 0> */
PUBLIC void switch_address_space(struct proc* p)
{
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
