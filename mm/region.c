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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "signal.h"
#include "errno.h"
#include "proto.h"
#include "region.h"
#include "const.h"

#define REGION_DEBUG 1

/**
 * <Ring 1> Create new virtual memory region.
 * @param  mp         Process the region belongs to.
 * @param  vir_base   Virtual base adress.
 * @param  vir_length Virtual memory length.
 * @return            The memory region created.
 */
PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length, int flags)
{
    struct vir_region * region = (struct vir_region *)alloc_vmem(sizeof(struct vir_region));

#if REGION_DEBUG
    printl("MM: region_new: allocated memory for virtual region at 0x%x\n", (int)region);
#endif

    if (region) {
        region->vir_addr = vir_base;
        region->length = vir_length;
        region->flags = flags;
        INIT_LIST_HEAD(&(region->phys_blocks));
    }
    
    return region;
}

/**
 * <Ring 1> Allocate physical memory for rp.
 */
PUBLIC int region_alloc_phys(struct vir_region * rp)
{
    int base = (int)rp->vir_addr;
    int len = rp->length, allocated_len = 0;
    int alignment = 0;

    struct phys_region * pr = NULL;
    list_for_each_entry(pr, &(rp->phys_blocks), list) {
        len -= pr->length;
        allocated_len += pr->length;
    }

    base += allocated_len;
    if (base % PG_SIZE != 0) {
        alignment = base - (base / PG_SIZE) * PG_SIZE;
        base -= alignment;
        len += alignment;
    }

    alignment = 0;
    int end = base + allocated_len + len;
    if (end % PG_SIZE != 0) 
    {
        alignment = ((end / PG_SIZE) + 1) * PG_SIZE - end;
        end += alignment;
        len += alignment;
    }

    void * paddr = (void *)alloc_pages(len / PG_SIZE);
    if (!paddr) return ENOMEM;

    struct phys_region * pregion = (struct phys_region *)alloc_vmem(sizeof(struct phys_region));

    if (!pregion) return ENOMEM;
    
    pregion->phys_addr = paddr;
    pregion->vir_addr = (void*)base;
    pregion->length = len;

    rp->length = allocated_len + len;

    list_add(&(pregion->list), &(rp->phys_blocks));

#if REGION_DEBUG
    printl("MM: region_alloc_phys: allocated physical memory for region: v: 0x%x ~ 0x%x(%x bytes), phys_base: 0x%x\n", base, end, len, paddr);
#endif

    return 0;
}

/**
 * <Ring 1> Map the physical memory of virtual region rp. 
 */
PUBLIC int region_map_phys(struct proc * mp, struct vir_region * rp)
{
    struct phys_region * pregion = NULL;
    void * phys_base = NULL;
    int len = 0, len_to_map = rp->length;

#if REGION_DEBUG
    printl("MM: region_map_phys: mapping virtual memory region(0x%x, 0x%x)\n", 
                (int)rp->vir_addr, (int)rp->vir_addr + rp->length);
#endif
    
    if (list_empty(&(rp->phys_blocks))) return ENOMEM;
    
    list_for_each_entry(pregion, &(rp->phys_blocks), list) {
        len = pregion->length;
        len_to_map -= len;
        if (pregion->flags & RF_MAPPED) continue;
        phys_base = pregion->phys_addr;
        pregion->flags |= RF_MAPPED;
        map_memory(&(mp->pgd), phys_base, pregion->vir_addr, len);
#if REGION_DEBUG
        printl("MM: region_map_phys: map physical memory region(0x%x - 0x%x) to virtual memory region(0x%x, 0x%x)\n", 
                (int)phys_base, (int)phys_base + len, (int)pregion->vir_addr, (int)pregion->vir_addr + len);
#endif
    }

    if (len_to_map > 0) {
        printl("MM: region_map_phys: No enough physical memory\n");
        return ENOMEM;
    }

    rp->flags |= RF_MAPPED;

    return 0;
}

/**
 * <Ring 1> Unmap the physical memory of virtual region rp. 
 */
PUBLIC int region_unmap_phys(struct proc * mp, struct vir_region * rp)
{
    struct phys_region * pregion = NULL;

    /* not mapped */
    if (!(rp->flags & RF_MAPPED)) return 0;

    unmap_memory(&(mp->pgd), rp->vir_addr, rp->length);

    list_for_each_entry(pregion, &(rp->phys_blocks), list) {
        pregion->flags &= ~RF_MAPPED;
    }

    rp->flags &= ~RF_MAPPED;

    return 0;
}

/**
 * <Ring 1> Extend memory region.
 */
PUBLIC int region_extend(struct vir_region * rp, int increment)
{
    if (increment % PG_SIZE != 0) {
        increment += PG_SIZE - (increment % PG_SIZE);
    }

    rp->length += increment;
    return region_alloc_phys(rp);
}

/**
 * <Ring 1> Extend stack.
 */
PUBLIC int region_extend_stack(struct vir_region * rp, int increment)
{
    /* make sure it's page-aligned */
    if (increment % PG_SIZE != 0) {
        increment += PG_SIZE - (increment % PG_SIZE);
    }

    rp->length += increment;
    rp->vir_addr = (void*)((int)(rp->vir_addr) - increment);

    void * paddr = (void *)alloc_pages(increment / PG_SIZE);
    if (!paddr) return ENOMEM;

    struct phys_region * pregion = (struct phys_region *)alloc_vmem(sizeof(struct phys_region));

    if (!pregion) return ENOMEM;
    
    pregion->phys_addr = paddr;
    pregion->vir_addr = rp->vir_addr;
    pregion->length = increment;

    list_add(&(pregion->list), &(rp->phys_blocks));

#if REGION_DEBUG
    printl("MM: region_extend_stack: extended stack by 0x%x bytes", increment);
#endif

    return 0;
}

PUBLIC int region_free(struct vir_region * rp)
{
    struct phys_region * pregion = NULL;

    if (!list_empty(&(rp->phys_blocks))) {
    
        list_for_each_entry(pregion, &(rp->phys_blocks), list) {
            if (&(pregion->list) != &(rp->phys_blocks) && &(pregion->list) != rp->phys_blocks.next) {
                free_vmem((int)list_entry(pregion->list.prev, struct phys_region, list), sizeof(struct phys_region));
            }
#if REGION_DEBUG
            free_mem((int)pregion->phys_addr, pregion->length);
            printl("MM: region_free: freed physical memory region(0x%x - 0x%x) \n", 
                (int)pregion->phys_addr, (int)pregion->phys_addr + pregion->length);
#endif
        }
        /* The last one */
        free_vmem((int)list_entry(pregion->list.prev, struct phys_region, list), sizeof(struct phys_region));
    }

    free_vmem((int)rp, sizeof(struct vir_region));
    rp = NULL;

    return 0;
}
