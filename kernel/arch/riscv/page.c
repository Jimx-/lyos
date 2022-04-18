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
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/page.h>
#include <asm/pagetable.h>
#include <errno.h>
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "asm/cpulocals.h"
#include <lyos/cpufeature.h>

extern char _VIR_BASE, _KERN_SIZE;

static phys_bytes kern_vir_base __attribute__((__section__(".unpaged_data"))) =
    (phys_bytes)&_VIR_BASE;
static phys_bytes kern_size __attribute__((__section__(".unpaged_data"))) =
    (phys_bytes)&_KERN_SIZE;

/* init page directory */
pde_t initial_pgd[ARCH_VM_DIR_ENTRIES]
    __attribute__((__section__(".unpaged_data")))
    __attribute__((aligned(ARCH_PG_SIZE)));
pde_t trampoline_pgd[ARCH_VM_DIR_ENTRIES]
    __attribute__((__section__(".unpaged_data")))
    __attribute__((aligned(ARCH_PG_SIZE)));
/* create page middle directories if it is not folded */
#ifndef __PAGETABLE_PMD_FOLDED
#define NUM_INIT_PMDS (((uintptr_t)-KERNEL_VMA >> ARCH_PGD_SHIFT) - 2)
pmd_t initial_pmd[ARCH_VM_PMD_ENTRIES * NUM_INIT_PMDS]
    __attribute__((__section__(".unpaged_data")))
    __attribute__((aligned(ARCH_PG_SIZE)));
pmd_t trampoline_pmd[ARCH_VM_PMD_ENTRIES]
    __attribute__((__section__(".unpaged_data")))
    __attribute__((aligned(ARCH_PG_SIZE)));
#endif

void pg_identity(pde_t* pgd) __attribute__((__section__(".unpaged_text")));
void pg_mapkernel(pde_t* pgd) __attribute__((__section__(".unpaged_text")));
void pg_load(pde_t* pgd) __attribute__((__section__(".unpaged_text")));

void setup_paging() __attribute__((__section__(".unpaged_text")));
void enable_paging() __attribute__((__section__(".unpaged_text")));

void setup_paging()
{
    extern char _lyos_start, _end;
    int i;
    uintptr_t pa = (uintptr_t)&_lyos_start;
    pgprot_t prot = __pgprot(pgprot_val(RISCV_PG_KERNEL) | _RISCV_PG_EXEC);

    kinfo.kernel_start_phys = (phys_bytes)pa;
    kinfo.kernel_end_phys = (phys_bytes)&_end;

    va_pa_offset = KERNEL_VMA - pa;

#ifndef __PAGETABLE_PMD_FOLDED
    trampoline_pgd[(KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES] =
        pfn_pde((uintptr_t)trampoline_pmd >> ARCH_PG_SHIFT,
                __pgprot(_RISCV_PG_TABLE));
    trampoline_pmd[0] = pfn_pmd((pa >> ARCH_PG_SHIFT), prot);

    for (i = 0; i < NUM_INIT_PMDS; i++) {
        initial_pgd[(KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES + i] =
            pfn_pde(((uintptr_t)initial_pmd >> ARCH_PG_SHIFT) + i,
                    __pgprot(_RISCV_PG_TABLE));
    }

    for (i = 0; i < sizeof(initial_pmd) / sizeof(pmd_t); i++) {
        initial_pmd[i] =
            pfn_pmd((pa + i * ARCH_PMD_SIZE) >> ARCH_PG_SHIFT, prot);
    }
#endif
}

void cut_memmap(kinfo_t* pk, phys_bytes start, phys_bytes end)
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

phys_bytes pg_alloc_page(kinfo_t* pk)
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

phys_bytes pg_alloc_lowest(kinfo_t* pk, phys_bytes size)
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

static pte_t* pg_alloc_pt(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_page(pk);
    pte_t* pt = __va(phys_addr);

    memset(pt, 0, sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
    return pt;
}

static pmd_t* pg_alloc_pmd(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_page(pk);
    pmd_t* pmd = __va(phys_addr);

    memset(pmd, 0, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
    return pmd;
}

void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t* pk)
{
    pde_t* pgd = initial_pgd;

    if (phys_addr % ARCH_PG_SIZE) phys_addr = roundup(phys_addr, ARCH_PG_SIZE);

    while (vir_addr < vir_end) {
        phys_bytes ph = phys_addr;
        if (ph == 0) ph = pg_alloc_page(pk);

        pde_t* pde = pgd_offset(pgd, (unsigned long)vir_addr);

        pud_t* pude = pud_offset(pde, (unsigned long)vir_addr);
        if (!pude_present(*pude)) {
            pmd_t* new_pmd = pg_alloc_pmd(pk);
            pude_populate(pude, __pa(new_pmd));
        }

        pmd_t* pmde = pmd_offset(pude, (unsigned long)vir_addr);
        if (!pmde_present(*pmde)) {
            pte_t* new_pt = pg_alloc_pt(pk);
            pmde_populate(pmde, __pa(new_pt));
        }

        pte_t* pte = pte_offset(pmde, (unsigned long)vir_addr);
        *pte = pfn_pte(ph >> ARCH_PG_SHIFT, RISCV_PG_EXEC_WRITE);

        vir_addr += ARCH_PG_SIZE;
        if (phys_addr != 0) phys_addr += ARCH_PG_SIZE;
    }
}

void pg_identity(pde_t* pgd) {}

void pg_mapkernel(pde_t* pgd) {}

void pg_load(pde_t* pgd) {}

void switch_address_space(struct proc* p)
{
    get_cpulocal_var(pt_proc) = p;
    write_ptbr(p->seg.ptbr_phys);
}
