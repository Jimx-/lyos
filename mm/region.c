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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "errno.h"
#include "region.h"
#include "proto.h"
#include "const.h"

//#define REGION_DEBUG 1

/**
 * @brief phys_region_free
 * @details Free a physical region.
 * 
 * @param phys_region The physical region to be freed.
 */
PRIVATE void phys_region_free(struct phys_region * rp)
{
    free_vmem((int)(rp->frames), rp->capacity * sizeof(struct phys_frame));
    rp->capacity = 0;
}

PUBLIC int phys_region_init(struct phys_region * rp, int capacity)
{
    int alloc_size = capacity * sizeof(struct phys_frame);
    if (alloc_size % PG_SIZE != 0) {
        alloc_size = (alloc_size / PG_SIZE + 1) * PG_SIZE;
        capacity = alloc_size / sizeof(struct phys_frame);
    }

    if (rp->capacity != 0) phys_region_free(rp);

    rp->frames = (struct phys_frame *)alloc_vmem(NULL, alloc_size);
    if (rp->frames == NULL) return ENOMEM;
    memset(rp->frames, 0, alloc_size);
    rp->capacity = capacity;

    return 0;
}

PRIVATE int phys_region_realloc(struct phys_region * rp, int new_capacity)
{
    int alloc_size = new_capacity * sizeof(struct phys_frame);
    if (alloc_size % PG_SIZE != 0) {
        alloc_size = (alloc_size / PG_SIZE + 1) * PG_SIZE;
        new_capacity = alloc_size / sizeof(struct phys_frame);
    }

    struct phys_frame * new_frames = (struct phys_frame *)alloc_vmem(NULL, alloc_size);
    if (new_frames == NULL) return ENOMEM;

    memcpy(new_frames, rp->frames, rp->capacity * sizeof(struct phys_frame));
    free_vmem((int)(rp->frames), rp->capacity * sizeof(struct phys_frame));
    rp->frames = new_frames;
    rp->capacity = new_capacity;

    return 0;
}

PRIVATE int phys_region_extend_up_to(struct phys_region * rp, int new_capacity)
{
    if (rp->capacity >= new_capacity) return 0;
    else return phys_region_realloc(rp, new_capacity);
}

PRIVATE int phys_region_right_shift(struct phys_region * rp, int new_start)
{
    struct phys_frame * start = &(rp->frames[new_start]);
    memmove(start, rp->frames, (rp->capacity - new_start) * sizeof(struct phys_frame));
    memset(rp->frames, 0, new_start * sizeof(struct phys_frame));

    return 0;
}

PRIVATE struct phys_frame * phys_region_get(struct phys_region * rp, int i)
{
    return &(rp->frames[i]);
}

/**
 * <Ring 1> Create new virtual memory region.
 * @param  mp         Process the region belongs to.
 * @param  vir_base   Virtual base adress.
 * @param  vir_length Virtual memory length.
 * @return            The memory region created.
 */
PUBLIC struct vir_region * region_new(struct proc * mp, void * vir_base, int vir_length, int flags)
{
    struct vir_region * region = (struct vir_region *)alloc_vmem(NULL, sizeof(struct vir_region));

#if REGION_DEBUG
    printl("MM: region_new: allocated memory for virtual region at 0x%x\n", (int)region);
#endif

    if (region) {
        region->vir_addr = vir_base;
        region->length = vir_length;
        region->flags = flags;
        struct phys_region * pr = &(region->phys_block);
        pr->capacity = 0;
        if (phys_region_init(pr, region->length / PG_SIZE) != 0) return NULL;
    }
    
    return region;
}

/**
 * <Ring 1> Allocate physical memory for rp.
 */
PUBLIC int region_alloc_phys(struct vir_region * rp)
{
    struct phys_region * pregion = &(rp->phys_block);
    int base = (int)rp->vir_addr, len = rp->length;
    int i;

    for (i = 0; len > 0; len -= PG_SIZE, base += PG_SIZE, i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt > 0 && frame->phys_addr != NULL) continue;
        void * paddr = (void *)alloc_pages(1);
        if (!paddr) return ENOMEM;
        frame->flags = RF_NORMAL | (rp->flags & RF_WRITABLE);
        frame->phys_addr = paddr; 
        frame->refcnt = 1;
    }
#if REGION_DEBUG
    printl("MM: region_alloc_phys: allocated physical memory for region: v: 0x%x ~ 0x%x(%x bytes)\n", rp->vir_addr, rp->vir_addr + rp->length, rp->length);
#endif

    return 0;
}

/**
 * <Ring 1> Map the physical memory of virtual region rp. 
 */
PUBLIC int region_map_phys(struct proc * mp, struct vir_region * rp)
{
#if REGION_DEBUG
    printl("MM: region_map_phys: mapping virtual memory region(0x%x - 0x%x)\n", 
                (int)rp->vir_addr, (int)rp->vir_addr + rp->length);
#endif
    
    struct phys_region * pregion = &(rp->phys_block);
    int base = (int)rp->vir_addr, len = rp->length;
    int i;

    for (i = 0; len > 0; len -= PG_SIZE, base += PG_SIZE, i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->phys_addr == NULL) continue;
#if REGION_DEBUG
        printl("MM: region_map_phys: mapping page(0x%x -> 0x%x)\n", 
                base, frame->phys_addr);
#endif
        int flags = PG_PRESENT | PG_USER;
        if (rp->flags & RF_WRITABLE) flags |= PG_RW;
        pt_mappage(&(mp->pgd), frame->phys_addr, (void*)base, flags);
        frame->flags |= RF_MAPPED;
    }

    rp->flags |= RF_MAPPED;

    return 0;
}

/**
 * <Ring 1> Unmap the physical memory of virtual region rp. 
 */
PUBLIC int region_unmap_phys(struct proc * mp, struct vir_region * rp)
{
    /* not mapped */
    if (!(rp->flags & RF_MAPPED)) return 0;

    unmap_memory(&(mp->pgd), rp->vir_addr, rp->length);

    struct phys_region * pregion = &(rp->phys_block);
    int i;

    for (i = 0; i < rp->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        frame->flags &= ~RF_MAPPED;
    }

    rp->flags &= ~RF_MAPPED;

    return 0;
}

/**
 * <Ring 1> Make virtual region rp write-protected. 
 */
PUBLIC int region_wp(struct proc * mp, struct vir_region * rp)
{
    pt_wp_memory(&(mp->pgd), rp->vir_addr, rp->length);
/*
    struct phys_region * pregion = &(rp->phys_block);
    int i;

    for (i = 0; i < rp->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        frame->flags |= ~RF_WRITEPROTECTED;
    }

    rp->flags &= ~RF_MAPPED;
*/
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
    int retval = phys_region_extend_up_to(&(rp->phys_block), rp->length / PG_SIZE);
    if (retval) return retval;
    return region_alloc_phys(rp);
}

/**
 * <Ring 1> Extend stack.
 */
PUBLIC int region_extend_stack(struct vir_region * rp, int increment)
{
    struct phys_region * pregion = &(rp->phys_block);

    /* make sure it's page-aligned */
    if (increment % PG_SIZE != 0) {
        increment += PG_SIZE - (increment % PG_SIZE);
    }

    rp->length += increment;
    rp->vir_addr = (void*)((int)(rp->vir_addr) - increment);

    int retval = phys_region_extend_up_to(pregion, rp->length / PG_SIZE);
    if(retval) return retval;
    phys_region_right_shift(pregion, increment / PG_SIZE);
    
    retval = region_alloc_phys(rp);

#if REGION_DEBUG
    printl("MM: region_extend_stack: extended stack by 0x%x bytes\n", increment);
#endif

    return retval;
}

PUBLIC int region_share(struct vir_region * dest, struct vir_region * src)
{
    int i;

    dest->vir_addr = src->vir_addr;
    dest->length = src->length;
    dest->flags = src->flags;

    struct phys_region * pregion = &(src->phys_block);
    for (i = 0; i < src->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt) frame->refcnt++;
        frame->flags |= RF_SHARED;
    }

    phys_region_free(&(dest->phys_block));

    dest->phys_block.capacity = pregion->capacity;
    dest->phys_block.frames = pregion->frames;

    return 0;
}

PUBLIC int region_free(struct vir_region * rp)
{
    struct phys_region * pregion = &(rp->phys_block);
    int free_frames = 1;
    int i;
    for (i = 0; i < rp->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt) frame->refcnt--;

        if (frame->refcnt <= 0) {
            if (frame->phys_addr) free_mem((int)(frame->phys_addr), PG_SIZE);
        } else {
            free_frames = 0;
        }
    }

    if (free_frames) phys_region_free(pregion);
    free_vmem((int)rp, sizeof(struct vir_region));
    rp = NULL;

    return 0;
}
