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

extern char _VIR_BASE, _KERN_SIZE;

PRIVATE phys_bytes kern_vir_base __attribute__ ((__section__(".unpaged_data"))) = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_size __attribute__ ((__section__(".unpaged_data"))) = (phys_bytes) &_KERN_SIZE;

/* init page directory */
PUBLIC pde_t initial_pgd[ARCH_VM_DIR_ENTRIES] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(ARCH_PG_SIZE)));
PUBLIC pde_t trampoline_pgd[ARCH_VM_DIR_ENTRIES] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(ARCH_PG_SIZE)));
/* create page middle directories if it is not folded */
#ifndef __PAGETABLE_PMD_FOLDED
#define NUM_INIT_PMDS ((uintptr_t)-KERNEL_VMA >> ARCH_PGD_SHIFT)
PUBLIC pmd_t initial_pmd[ARCH_VM_PMD_ENTRIES * NUM_INIT_PMDS] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(ARCH_PG_SIZE)));
PUBLIC pmd_t trampoline_pmd[ARCH_VM_PMD_ENTRIES] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(ARCH_PG_SIZE)));
#endif

PUBLIC void pg_identity(pde_t * pgd) __attribute__((__section__(".unpaged_text")));
PUBLIC void pg_mapkernel(pde_t * pgd) __attribute__((__section__(".unpaged_text")));
PUBLIC void pg_load(pde_t * pgd) __attribute__((__section__(".unpaged_text")));

PUBLIC void setup_paging() __attribute__((__section__(".unpaged_text")));
PUBLIC void enable_paging() __attribute__((__section__(".unpaged_text")));

PUBLIC void setup_paging()
{
    extern char _lyos_start, _end;
    int i;
    uintptr_t pa = (uintptr_t) &_lyos_start;
    pgprot_t prot = __pgprot(pgprot_val(RISCV_PG_KERNEL) | _RISCV_PG_EXEC);

    kinfo.kernel_start_phys = (phys_bytes) pa;
    kinfo.kernel_end_phys = (phys_bytes) &_end;

    va_pa_offset = KERNEL_VMA - pa;

#ifndef __PAGETABLE_PMD_FOLDED
    trampoline_pgd[(KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES] =
        pfn_pde((uintptr_t) trampoline_pmd >> ARCH_PG_SHIFT, __pgprot(_RISCV_PG_TABLE));
    trampoline_pmd[0] = pfn_pmd((pa >> ARCH_PG_SHIFT), prot);

    for (i = 0; i < NUM_INIT_PMDS; i++) {
        initial_pgd[(KERNEL_VMA >> ARCH_PGD_SHIFT) % ARCH_VM_DIR_ENTRIES + i] =
            pfn_pde(((uintptr_t) initial_pmd >> ARCH_PG_SHIFT) + i, __pgprot(_RISCV_PG_TABLE));
    }

    for (i = 0; i < sizeof(initial_pmd) / sizeof(pmd_t); i++) {
        initial_pmd[i] = pfn_pmd((pa + i * ARCH_PMD_SIZE) >> ARCH_PG_SHIFT, prot);
    }
#endif
}

PUBLIC void cut_memmap(kinfo_t * pk, phys_bytes start, phys_bytes end)
{
    int i;

    for (i = 0; i < pk->memmaps_count; i++) {
        struct kinfo_mmap_entry * entry = &pk->memmaps[i];
        u64 mmap_end = entry->addr + entry->len;
        if (start >= entry->addr && end <= mmap_end) {
            entry->addr = end;
            entry->len = mmap_end - entry->addr;
        }
    }
}

PUBLIC phys_bytes pg_alloc_page(kinfo_t * pk)
{
    int i;

    for (i = pk->memmaps_count - 1; i >= 0; i--) {
        struct kinfo_mmap_entry * entry = &pk->memmaps[i];

        if (entry->type != KINFO_MEMORY_AVAILABLE) continue;

        if (!(entry->addr % ARCH_PG_SIZE) && (entry->len >= ARCH_PG_SIZE)) {
            entry->addr += ARCH_PG_SIZE;
            entry->len -= ARCH_PG_SIZE;

            return entry->addr - ARCH_PG_SIZE;
        }
    }

    return 0;
}

PRIVATE pte_t* pg_alloc_pt(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_page(pk);
    pte_t* pt = __va(phys_addr);

    memset(pt, 0, sizeof(pte_t) * ARCH_VM_PT_ENTRIES);
    return pt;
}

PRIVATE pmd_t* pg_alloc_pmd(kinfo_t* pk)
{
    phys_bytes phys_addr = pg_alloc_page(pk);
    pmd_t* pmd = __va(phys_addr);

    memset(pmd, 0, sizeof(pmd_t) * ARCH_VM_PMD_ENTRIES);
    return pmd;
}

PRIVATE inline pde_t* pgd_offset(pde_t* pgd, unsigned long addr)
{
    return pgd + ARCH_PDE(addr);
}

PRIVATE inline int pde_present(pde_t pde)
{
    return pde_val(pde) & _RISCV_PG_PRESENT;
}

PRIVATE inline void pde_populate(pde_t* pde, pmd_t* pmd)
{
    unsigned long pfn = __pa(pmd) >> ARCH_PG_SHIFT;
    *pde = __pde((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

PRIVATE inline pmd_t* pmd_offset(pde_t* pmd, unsigned long addr)
{
    pmd_t* vaddr = (pmd_t*) __va((pde_val(*pmd) >> RISCV_PG_PFN_SHIFT) << ARCH_PG_SHIFT);
    return vaddr + ARCH_PMDE(addr);
}

PRIVATE inline int pmde_present(pmd_t pmde)
{
    return pmd_val(pmde) & _RISCV_PG_PRESENT;
}

PRIVATE inline void pmde_populate(pmd_t* pmde, pte_t* pt)
{
    unsigned long pfn = __pa(pt) >> ARCH_PG_SHIFT;
    *pmde = __pmd((pfn << RISCV_PG_PFN_SHIFT) | _RISCV_PG_TABLE);
}

PRIVATE inline pte_t* pte_offset(pmd_t* pt, unsigned long addr)
{
    pte_t* vaddr = (pte_t*) __va((pmd_val(*pt) >> RISCV_PG_PFN_SHIFT) << ARCH_PG_SHIFT);
    return vaddr + ARCH_PTE(addr);
}

PRIVATE inline int pte_present(pte_t pte)
{
    return pte_val(pte) & _RISCV_PG_PRESENT;
}

PUBLIC void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t * pk)
{
    pde_t* pgd = initial_pgd;

    if (phys_addr % ARCH_PG_SIZE) phys_addr = roundup(phys_addr, ARCH_PG_SIZE);

    while (vir_addr < vir_end) {
        phys_bytes ph = phys_addr;
        if (ph == 0) ph = pg_alloc_page(pk);

        pde_t* pde = pgd_offset(pgd, (unsigned long) vir_addr);
        if (!pde_present(*pde)) {
            pmd_t* new_pmd = pg_alloc_pmd(pk);
            pde_populate(pde, new_pmd);
        }

        pmd_t* pmde = pmd_offset(pde, (unsigned long) vir_addr);
        if (!pmde_present(*pmde)) {
            pte_t* new_pt = pg_alloc_pt(pk);
            pmde_populate(pmde, new_pt);
        }

        pte_t* pte = pte_offset(pmde, (unsigned long) vir_addr);
        *pte = pfn_pte(ph >> ARCH_PG_SHIFT, RISCV_PG_EXEC_WRITE);

        vir_addr += ARCH_PG_SIZE;
        if (phys_addr != 0) phys_addr += ARCH_PG_SIZE;
    }
}

PUBLIC void pg_identity(pde_t * pgd)
{
}

PUBLIC void pg_mapkernel(pde_t * pgd)
{
}

PUBLIC void pg_load(pde_t * pgd)
{
}

PUBLIC void switch_address_space(struct proc * p)
{
}
