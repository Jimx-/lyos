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

static void __mmput(struct mm_struct* mm)
{
    struct vir_region *vr, *tmp;

    if (!list_empty(&mm->mem_regions)) {
        list_for_each_entry_safe(vr, tmp, &mm->mem_regions, list)
        {
            region_free(vr);
        }
    }

    pgd_free(&mm->pgd);
    mm_free(mm);
}

void mmput(struct mm_struct* mm)
{
    if (atomic_dec_and_test(&mm->refcnt)) {
        __mmput(mm);
    }
}

/*****************************************************************************
 *                                do_fork
 *****************************************************************************/
/**
 * Perform the memory part of fork() syscall.
 *
 * @return  Zero if success, otherwise -1.
 *****************************************************************************/
int do_fork()
{
    endpoint_t parent_ep = mm_msg.ENDPOINT;
    int child_slot = mm_msg.PROC_NR;
    void* newsp = mm_msg.BUF;
    int flags = mm_msg.FLAGS;
    endpoint_t child_ep;
    struct mmproc* mmp = &mmproc_table[child_slot];

    /* duplicate the process table */
    int parent_slot, retval;
    if ((retval = mm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct mmproc* mmparent = mmproc_table + parent_slot;

    *mmp = mmproc_table[parent_slot];
    mmp->flags |= MMPF_INUSE;

    int kfork_flags = KF_MMINHIBIT;
    if ((retval = kernel_fork(parent_ep, child_slot, &child_ep, kfork_flags,
                              newsp)) != 0)
        return retval;
    mmp->endpoint = child_ep;

    int clone_vm = flags & CLONE_VM;

    if (clone_vm) { /* share address space */
        mmp->mm = mmparent->mm;
        mmget(mmp->mm);
        if (pgd_bind(mmp, &mmp->mm->pgd))
            panic("MM: fork: cannot bind new pgdir");
    } else {
        if ((mmp->mm = mm_allocate()) == NULL) return -ENOMEM;
        mm_init(mmp->mm);
        mmp->mm->slot = child_slot;

        if (pgd_new(&mmp->mm->pgd) != 0) {
            printl("MM: fork: can't create new page directory.\n");
            return -ENOMEM;
        }

        if (pgd_bind(mmp, &mmp->mm->pgd))
            panic("MM: fork: cannot bind new pgdir");

        /* copy regions */
        struct vir_region* vr;
        list_for_each_entry(vr, &mmparent->mm->mem_regions, list)
        {
            struct vir_region* new_region =
                region_new(vr->vir_addr, vr->length, vr->flags);
            list_add(&(new_region->list), &mmp->mm->mem_regions);
            avl_insert(&new_region->avl, &mmp->mm->mem_avl);

            if (vr->flags & RF_FILEMAP) {
                new_region->param.file = vr->param.file;
                file_reference(new_region, vr->param.file.filp);
            }

            if (!(vr->flags & RF_MAPPED)) continue;

            region_share(mmp, new_region, mmparent, vr, FALSE);
            region_map_phys(mmp, new_region);
        }
    }

    mmp->group_leader = mmp;
    INIT_LIST_HEAD(&mmp->group_list);

    /* add child to process group */
    if (flags & CLONE_VM) {
        mmp->group_leader = mmparent->group_leader;
        if (mmparent->group_leader == NULL)
            panic("do_fork(): BUG: parent has no group leader\n");
        list_add(&mmp->group_list, &mmp->group_leader->group_list);
    }

    /* child PID will be returned to the parent proc */
    mm_msg.ENDPOINT = mmp->endpoint;

    return 0;
}

int proc_free(struct mmproc* mmp, int clear_proc)
{
    struct vir_region* vr;

    /* free memory */
    if (clear_proc) {
        mmput(mmp->mm);
        mmp->mm = NULL;

        mmp->flags &= ~MMPF_INUSE;
    } else { /* clear mem regions only */

        /*pgd_clear(&mmp->mm->pgd);
        pgd_mapkernel(&mmp->mm->pgd);*/

        if (!list_empty(&mmp->mm->mem_regions)) {
            struct vir_region* tmp;
            list_for_each_entry_safe(vr, tmp, &mmp->mm->mem_regions, list)
            {
                region_unmap_phys(mmp, vr);
                pgd_free_range(&mmp->mm->pgd, vr->vir_addr,
                               vr->vir_addr + vr->length, vr->vir_addr,
                               vr->vir_addr + vr->length);
                region_free(vr);
            }
        }

        INIT_LIST_HEAD(&mmp->mm->mem_regions);
        region_init_avl(mmp->mm);
    }

    return 0;
}
