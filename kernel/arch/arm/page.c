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

extern char _PHYS_BASE, _VIR_BASE, _KERN_SIZE;

PRIVATE phys_bytes kern_vir_base = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_phys_base = (phys_bytes) &_PHYS_BASE;
PRIVATE phys_bytes kern_size = (phys_bytes) &_KERN_SIZE;

PRIVATE pde_t initial_pgd[ARCH_VM_DIR_ENTRIES] __attribute__ ((__section__(".unpaged_data"))) __attribute__((aligned(16384)));

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
    sctlr &= 0xffff;
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 0 @ Write SCTLR\n\t"
            : : [ctl] "r" (sctlr));
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
    while(mapped_size < kern_size) {
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

PUBLIC void switch_address_space(struct proc * p) {

}
