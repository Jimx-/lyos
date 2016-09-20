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
#include <sched.h>
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
    void* newsp = mm_msg.BUF;
    int flags = mm_msg.FLAGS;
    endpoint_t child_ep;
    struct mmproc * mmp = &mmproc_table[child_slot];

    /* duplicate the process table */
    int parent_slot, retval;
    if ((retval = mm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct mmproc * mmparent = mmproc_table + parent_slot;
    
    *mmp = mmproc_table[parent_slot];
    mmp->flags &= MMPF_INUSE;

    int kfork_flags = KF_MMINHIBIT;
    if ((retval = kernel_fork(parent_ep, child_slot, &child_ep, kfork_flags, newsp)) != 0) return retval;
    mmp->endpoint = child_ep;

    int clone_vm = flags & CLONE_VM;

    if (clone_vm) {     /* share address space */
        mmp->mm = NULL;
        mmp->active_mm = mmparent->active_mm;
        atomic_inc(&mmp->active_mm->refcnt);
        if (pgd_bind(mmp, &mmp->active_mm->pgd)) panic("MM: fork: cannot bind new pgdir");
    } else {
        if ((mmp->mm = mm_allocate()) == NULL) return -ENOMEM;
        mm_init(mmp->mm);
        mmp->mm->slot = child_slot;
        mmp->active_mm = mmp->mm;

        if (pgd_new(&mmp->mm->pgd) != 0) {
            printl("MM: fork: can't create new page directory.\n");
            return -ENOMEM;
        }

        if (pgd_bind(mmp, &mmp->mm->pgd)) panic("MM: fork: cannot bind new pgdir");

        /* copy regions */
        struct vir_region * vr;
        list_for_each_entry(vr, &mmparent->active_mm->mem_regions, list) {
            struct vir_region * new_region = region_new(vr->vir_addr, vr->length, vr->flags);
            list_add(&(new_region->list), &mmp->active_mm->mem_regions);
           
            if (vr->flags & RF_FILEMAP) {
                new_region->param.file = vr->param.file;
                file_reference(new_region, vr->param.file.filp);
            }

            if (!(vr->flags & RF_MAPPED)) continue;

            region_share(mmp, new_region, mmparent, vr);
            region_map_phys(mmp, new_region);
        }
    }

    mmp->group_leader = mmp;
    INIT_LIST_HEAD(&mmp->group_list);

    /* add child to process group */
    if (flags & CLONE_VM) {
        mmp->group_leader = mmparent->group_leader;
        if (mmparent->group_leader == NULL) panic("do_fork(): BUG: parent has no group leader\n");
        list_add(&mmp->group_list, &mmp->group_leader->group_list);
    }

    /* child PID will be returned to the parent proc */
    mm_msg.ENDPOINT = mmp->endpoint;
    
    return 0;
}

PUBLIC int proc_free(struct mmproc * mmp, int clear_proc)
{
    /* free memory */
    struct vir_region * vr;

    mmp->mmap_base = ARCH_BIG_PAGE_SIZE;
    if (mmp->mm == NULL) {
        atomic_dec(&mmp->active_mm->refcnt);
        mmp->active_mm = NULL;
        return 0;
    }

    if (!list_empty(&mmp->mm->mem_regions)) {
        struct vir_region* tmp;
        list_for_each_entry_safe(vr, tmp, &mmp->mm->mem_regions, list) {
            region_unmap_phys(mmp, vr);
            region_free(vr);
        }
    }

    if (clear_proc) {
        pgd_free(&(mmp->mm->pgd));
        mm_free(mmp->mm);
        mmp->mm = mmp->active_mm = NULL;
    } else {   /* clear mem regions only */
        /*pgd_clear(&mmp->mm->pgd);
        pgd_mapkernel(&mmp->mm->pgd);*/

        INIT_LIST_HEAD(&mmp->mm->mem_regions);
    }

    return 0;
}
