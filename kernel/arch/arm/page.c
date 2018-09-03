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
#include "page.h"
#include <errno.h>
#include "arch_proto.h"
#include "arch_const.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/cpufeature.h>

extern char _PHYS_BASE, _VIR_BASE, _KERN_SIZE, _KERN_OFFSET;

/* paging utilities */
PRIVATE phys_bytes kern_vir_base __attribute__ ((__section__(".unpaged_data"))) = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_phys_base __attribute__ ((__section__(".unpaged_data"))) = (phys_bytes) &_PHYS_BASE;
PRIVATE phys_bytes kern_size __attribute__ ((__section__(".unpaged_data"))) = (phys_bytes) &_KERN_SIZE;

PUBLIC pde_t initial_pgd[ARCH_VM_DIR_ENTRIES] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(16384)));

PUBLIC void pg_identity(pde_t * pgd) __attribute__((__section__(".unpaged_text")));
PUBLIC pde_t pg_mapkernel(pde_t * pgd) __attribute__((__section__(".unpaged_text")));
PUBLIC void pg_load(pde_t * pgd) __attribute__((__section__(".unpaged_text")));

PUBLIC void setup_paging() __attribute__((__section__(".unpaged_text")));
PUBLIC void enable_paging() __attribute__((__section__(".unpaged_text")));

PUBLIC void setup_paging()
{
    pg_identity(initial_pgd);
    int freepde = pg_mapkernel(initial_pgd);
    pg_load(initial_pgd);
    enable_paging();
}

/* CR1 bits */
#define CR_M    (1 << 0)    /* Enable MMU */    
#define CR_C    (1 << 2)    /* Data cache */
#define CR_Z    (1 << 11)   /* Branch Predict */
#define CR_I    (1 << 12)   /* Instruction cache */
#define CR_TRE  (1 << 28)   /* TRE: TEX Remap*/
#define CR_AFE  (1 << 29)   /* AFE: Access Flag enable */
PUBLIC void enable_paging()
{
    asm volatile("mcr p15, 0, %[bcr], c2, c0, 2 @ Write TTBCR\n\t"
            : : [bcr] "r" (0)); /* TTBCR = 0: always use TTBR0 */
    asm volatile("mcr p15, 0, %[dacr], c3, c0, 0 @ Write DACR\n\t"
            : : [dacr] "r" (0x55555555));   /* all client */

    u32 sctlr;
    asm volatile("mrc p15, 0, %[ctl], c1, c0, 0 @ Read SCTLR\n\t"
            : [ctl] "=r" (sctlr));
    sctlr |= CR_M;
    sctlr |= CR_C;
    sctlr |= CR_Z;
    sctlr |= CR_I;
    sctlr &= ~CR_TRE;
    sctlr &= ~CR_AFE;
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 0 @ Write SCTLR\n\t"
            : : [ctl] "r" (sctlr));
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

PRIVATE pte_t * pg_alloc_pt(phys_bytes * ph)
{
    pte_t * ret;
#define PG_PAGETABLES 6
    static pte_t pagetables[PG_PAGETABLES][1024]  __attribute__((aligned(ARCH_PG_SIZE)));
    static int pt_inuse = 0;

    if(pt_inuse >= PG_PAGETABLES) panic("no more pagetables");

    ret = pagetables[pt_inuse++];
    *ph = (phys_bytes)ret - (phys_bytes) &_KERN_OFFSET;

    return ret;
}

PUBLIC void pg_map(phys_bytes phys_addr, void* vir_addr, void* vir_end, kinfo_t * pk)
{
    pte_t * pt;
    pde_t * pgd = (pde_t *)((phys_bytes)initial_pgd + (phys_bytes) &_KERN_OFFSET); 
    if (phys_addr % PG_SIZE) phys_addr = (phys_addr / PG_SIZE) * PG_SIZE;
    if (vir_addr % PG_SIZE) vir_addr = (vir_addr / PG_SIZE) * PG_SIZE;

    while (vir_addr < vir_end) {
        phys_bytes phys = phys_addr;
        if (phys == 0) phys = pg_alloc_page(pk);

        int pde = ARCH_PDE(vir_addr);
        int pte = ARCH_PTE(vir_addr);

        if (pgd[pde] & ARM_VM_SECTION) {
            phys_bytes pt_ph;
            pt = pg_alloc_pt(&pt_ph);
            pgd[pde] = (pt_ph & ARM_VM_PDE_MASK)
                    | ARM_VM_PAGEDIR
                    | ARM_VM_PDE_DOMAIN;
        } else {
            pt = (pte_t *)(((phys_bytes)pgd[pde] & ARCH_VM_ADDR_MASK) + (phys_bytes) &_KERN_OFFSET);
        }

        pt[pte] = (phys & ARM_VM_ADDR_MASK)
            | ARM_PG_PRESENT
            | ARM_PG_CACHED
            | ARM_PG_USER;
        vir_addr += ARCH_PG_SIZE;
        if (phys_addr != 0) phys_addr += ARCH_PG_SIZE;
    }
}

PUBLIC void pg_identity(pde_t * pgd)
{
    int i;
    phys_bytes phys;

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        int flags = ARM_VM_SECTION | ARM_VM_SECTION_USER | ARM_VM_SECTION_DOMAIN;

        phys = i * ARM_SECTION_SIZE;

        pgd[i] = phys | flags;
    }
}

PUBLIC pde_t pg_mapkernel(pde_t * pgd)
{
    int pde;
    phys_bytes mapped_size = 0, kern_phys = kern_phys_base;

    pde = kern_vir_base / ARM_SECTION_SIZE;
    while (mapped_size < kern_size) {
        pgd[pde] = (kern_phys & ARM_VM_SECTION_MASK) 
            | ARM_VM_SECTION
            | ARM_VM_SECTION_SUPER
            | ARM_VM_SECTION_DOMAIN
            | ARM_VM_SECTION_CACHED;
        mapped_size += ARM_SECTION_SIZE;
        kern_phys += ARM_SECTION_SIZE;
        pde++;
    }

    return pde;
}

PUBLIC void pg_load(pde_t * pgd)
{
    u32 bar = (u32)pgd;
    asm volatile("mcr p15, 0, %[bar], c2, c0, 0 @ Write TTBR0\n\t"
            : : [bar] "r" (bar));
}

PUBLIC void reload_ttbr0()
{
    u32 bar = read_ttbr0();
    write_ttbr0(bar);
}

PUBLIC void switch_address_space(struct proc * p) 
{
    get_cpulocal_var(pt_proc) = p;
    u32 bar = p->seg.ttbr_phys;
    write_ttbr0(bar);
}
