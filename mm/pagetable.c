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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "region.h"
#include "proto.h"

//#define PAGETABLE_DEBUG    1

PUBLIC int pt_create(struct page_directory * pgd, int pde, u32 flags)
{
    pte_t * pt = (pte_t *)alloc_vmem(PT_SIZE);
    if (pt == NULL) {
        printl("MM: pt_create: failed to allocate memory for new page table\n");
        return ENOMEM;
    }

#if PAGETABLE_DEBUG
        printl("MM: pt_create: allocated new page table\n");
#endif

    int i;
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pt[i] = 0;
    }

    pgd->vir_addr[pde] = (int)va2pa(getpid(), pt) | flags;
    pgd->vir_pts[pde] = pt;

    return 0;
}

/**
 * <Ring 1> Map a physical page, create page table if necessary.
 * @param  phys_addr Physical address.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_mappage(struct page_directory * pgd, void * phys_addr, void * vir_addr, u32 flags)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];

    /* page table not present */
    if (pt == NULL) {
        int retval = pt_create(pgd, pgd_index, PG_PRESENT | PG_RW | PG_USER);

        if (retval) return retval;
        pt = pgd->vir_pts[pgd_index];
    }

    pt[pt_index] = ((int)phys_addr & 0xFFFFF000) | flags;

    return 0;
}

/**
 * <Ring 1> Make a physical page write-protected.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_wppage(struct page_directory * pgd, void * vir_addr)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];
    if (pt) pt[pt_index] &= ~PG_RW;

    return 0;
}

/**
 * <Ring 1> Make a physical page read-write.
 * @param  vir_addr  Virtual address.
 * @return           Zero on success.
 */
PUBLIC int pt_unwppage(struct page_directory * pgd, void * vir_addr)
{
    unsigned long pgd_index = ARCH_PDE(vir_addr);
    unsigned long pt_index = ARCH_PTE(vir_addr);

    pte_t * pt = pgd->vir_pts[pgd_index];
    if (pt) pt[pt_index] |= PG_RW;

    return 0;
}

PUBLIC int map_memory(struct page_directory * pgd, void * phys_addr, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)phys_addr % PG_SIZE != 0) printl("MM: map_memory: phys_addr is not page-aligned!\n");
    if ((int)vir_addr % PG_SIZE != 0) printl("MM: map_memory: vir_addr is not page-aligned!\n");
    if (length % PG_SIZE != 0) printl("MM: map_memory: length is not page-aligned!\n");
    
    while (1) {
        pt_mappage(pgd, phys_addr, vir_addr, PG_PRESENT | PG_RW | PG_USER);

        length -= PG_SIZE;
        phys_addr += PG_SIZE;
        vir_addr += PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

PUBLIC int pt_wp_memory(struct page_directory * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)vir_addr % PG_SIZE != 0) printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % PG_SIZE != 0) printl("MM: pt_wp_memory: length is not page-aligned!\n");
    
    while (1) {
        pt_wppage(pgd, vir_addr);

        length -= PG_SIZE;
        vir_addr += PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

PUBLIC int pt_unwp_memory(struct page_directory * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)vir_addr % PG_SIZE != 0) printl("MM: pt_wp_memory: vir_addr is not page-aligned!\n");
    if (length % PG_SIZE != 0) printl("MM: pt_wp_memory: length is not page-aligned!\n");
    
    while (1) {
        pt_unwppage(pgd, vir_addr);

        length -= PG_SIZE;
        vir_addr += PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

PUBLIC int unmap_memory(struct page_directory * pgd, void * vir_addr, int length)
{
    /* sanity check */
    if ((int)vir_addr % PG_SIZE != 0) printl("MM: map_memory: vir_addr is not page-aligned!\n");
    if (length % PG_SIZE != 0) printl("MM: map_memory: length is not page-aligned!\n");

    while (1) {
        pt_mappage(pgd, NULL, vir_addr, 0);

        length -= PG_SIZE;
        vir_addr += PG_SIZE;
        if (length <= 0) break;
    }

    return 0;
}

/* <Ring 1> */
PUBLIC int pgd_new(struct page_directory * pgd)
{
    /* map the directory so that we can write it */
    pde_t * pg_dir = (pde_t *)alloc_vmem(PGD_SIZE);

    pgd->phys_addr = va2pa(getpid(), pg_dir);
    pgd->vir_addr = pg_dir;

    int i;

    /* zero it */
    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pg_dir[i] = 0;
    }

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        pgd->vir_pts[i] = NULL;
    }

    pgd_mapkernel(pgd);
    return 0;
}

/**
 * <Ring 1> Map the kernel.
 * @param  pgd The page directory.
 * @return     Zero on success.
 */
PUBLIC int pgd_mapkernel(struct page_directory * pgd)
{
    int i;
    int kernel_pde = ARCH_PDE(KERNEL_VMA);

    for (i = 0; i < ARCH_PDE(VMALLOC_START) - kernel_pde; i++) {
        pgd->vir_addr[kernel_pde + i] = initial_pgd[i];
        pgd->vir_pts[kernel_pde + i] = (pte_t *)((initial_pgd[i] + KERNEL_VMA) & ARCH_VM_ADDR_MASK);
    }
    return 0;
}

/* <Ring 1> */
PUBLIC int pgd_clear(struct page_directory * pgd)
{
    int i;

    for (i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
        if (i >= ARCH_PDE(KERNEL_VMA) && i < ARCH_PDE(VMALLOC_START)) continue;  /* never unmap kernel */
        if (pgd->vir_pts[i]) {
            free_vmem((int)(pgd->vir_pts[i]), PT_SIZE);
        }
        pgd->vir_pts[i] = NULL;
    }

    return 0;
}
