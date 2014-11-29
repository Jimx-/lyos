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
#include "sys/wait.h"
#include "region.h"
#include "const.h"
#include "proto.h"
#include "global.h"
#include <lyos/sysutils.h>

/*****************************************************************************
 *                                do_fork
 *****************************************************************************/
/**
 * Perform the memory part of fork() syscall.
 * 
 * @return  Zero if success, otherwise -1.
 *****************************************************************************/
PUBLIC int do_fork()
{
	endpoint_t parent_ep = mm_msg.ENDPOINT;
	int child_slot = mm_msg.PROC_NR;
	endpoint_t child_ep;
	struct mmproc * mmp = &mmproc_table[child_slot];

	/* duplicate the process table */
	int parent_slot, retval;
	if ((retval = mm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
	struct mmproc * mmparent = mmproc_table + parent_slot;
	
	*mmp = mmproc_table[parent_slot];
	mmp->slot = child_slot;
	mmp->flags &= MMPF_INUSE;

	if ((retval = kernel_fork(parent_ep, child_slot, &child_ep, KF_MMINHIBIT)) != 0) return retval;
	mmp->endpoint = child_ep;

	if (pgd_new(&(mmp->pgd)) != 0) {
		printl("MM: fork: can't create new page directory.\n");
		return -ENOMEM;
	}

	if (pgd_bind(mmp, &mmp->pgd)) panic("MM: fork: cannot bind new pgdir");

	INIT_LIST_HEAD(&(mmp->mem_regions));

	/* copy regions */
	struct vir_region * vr;
    list_for_each_entry(vr, &(mmparent->mem_regions), list) {
    	struct vir_region * new_region = region_new(vr->vir_addr, vr->length, vr->flags);
       	list_add(&(new_region->list), &(mmp->mem_regions));
       	
       	if (!(vr->flags & RF_MAPPED)) continue;

       	if (vr->flags & RF_WRITABLE) {
       		region_alloc_phys(new_region);
       		region_map_phys(mmp, new_region);

       		data_copy(child_ep, new_region->vir_addr, parent_ep, vr->vir_addr, vr->length);
       	} else {	/* can be shared */
       		region_share(new_region, vr);
       		region_map_phys(mmp, new_region);
       	}
    }

	/* child PID will be returned to the parent proc */
	mm_msg.ENDPOINT = mmp->endpoint;

	return 0;
}

PUBLIC int proc_free(struct mmproc * mmp)
{
	/* free memory */
	struct vir_region * vr;

    if (!list_empty(&(mmp->mem_regions))) {
	    list_for_each_entry(vr, &(mmp->mem_regions), list) {
    		if ((&(vr->list) != &(mmp->mem_regions)) && (&(vr->list) != mmp->mem_regions.next)) {
    			region_unmap_phys(mmp, list_entry(vr->list.prev, struct vir_region, list));
       			region_free(list_entry(vr->list.prev, struct vir_region, list));
       		}
    	}
    }
    region_unmap_phys(mmp, list_entry(vr->list.prev, struct vir_region, list));
    region_free(list_entry(vr->list.prev, struct vir_region, list)); 
    INIT_LIST_HEAD(&(mmp->mem_regions));
    pgd_clear(&(mmp->pgd));

    return 0;
}
