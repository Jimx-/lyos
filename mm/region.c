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

#define REGION_DEBUG 1

/**
 * <Ring 1> Create new virtual memory region.
 * @param  mp         Process the region belongs to.
 * @param  vir_base   Virtual base adress.
 * @param  vir_length Virtual memory length.
 * @return            The memory region created.
 */
PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length)
{
    struct vir_region * region = (struct vir_region *)alloc_vmem(sizeof(struct vir_region));

#if REGION_DEBUG
    printl("MM: region_new: allocated memory for virtual region at 0x%x\n", (int)region);
#endif

    if (region) {
        region->vir_addr = vir_base;
        region->length = vir_length;
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
    int len = rp->length;
    int alignment = 0;

    if (base % PG_SIZE != 0) {
        alignment = base - (base / PG_SIZE) * PG_SIZE;
        base -= alignment;
        len += alignment;
    }

    alignment = 0;
    int end = base + len;
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
    pregion->length = len;

    list_add(&(pregion->list), &(rp->phys_blocks));

#if REGION_DEBUG
    printl("MM: region_alloc_phys: allocated physical memory for text segment: v: 0x%x ~ 0x%x(%x bytes), phys_base: 0x%x\n", base, end, len, paddr);
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
    int len = 0;

    if (list_empty(&(rp->phys_blocks))) return ENOMEM;
    
    list_for_each_entry(pregion, &(rp->phys_blocks), list) {
        phys_base = pregion->phys_addr;
        len = pregion->length;
    }
    
    void * vir_base = (void *)(((int)rp->vir_addr / PG_SIZE) * PG_SIZE);
    map_memory(&(mp->pgd), phys_base, vir_base, len);

#if REGION_DEBUG
    printl("MM: region_map_phys: map physical memory region(0x%x - 0x%x) to virtual memory region(0x%x, 0x%x)\n", 
                (int)phys_base, (int)phys_base + len, (int)vir_base, (int)vir_base + len);
#endif

    return 0;
}