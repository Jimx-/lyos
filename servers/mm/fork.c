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
#include "signal.h"
#include "errno.h"
#include "sys/wait.h"
#include <sched.h>
#include "region.h"
#include "const.h"
#include "proto.h"
#include "global.h"
#include <lyos/sysutils.h>

static inline void __mmput(struct mm_struct* mm)
{
    region_free_mm(mm);
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
    struct mm_fork_info* mfi = (struct mm_fork_info*)mm_msg.MSG_PAYLOAD;
    endpoint_t parent_ep = mfi->parent;
    int child_slot = mfi->slot;
    void* newsp = mfi->newsp;
    void* tls = mfi->tls;
    int flags = mfi->flags;
    endpoint_t child_ep;
    struct mmproc* mmp = &mmproc_table[child_slot];
    int kfork_flags;

    /* duplicate the process table */
    int parent_slot, retval;
    if ((retval = mm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct mmproc* mmparent = mmproc_table + parent_slot;

    *mmp = mmproc_table[parent_slot];
    mmp->flags |= MMPF_INUSE;

    kfork_flags = KF_MMINHIBIT;
    if (flags & CLONE_SETTLS) kfork_flags |= KF_SETTLS;

    if ((retval = kernel_fork(parent_ep, child_slot, &child_ep, kfork_flags,
                              newsp, tls)) != 0)
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

        if (region_copy_proc(mmp, mmparent) != OK) {
            mmput(mmp->mm);
            mmp->mm = NULL;
            return ENOMEM;
        }

        if (pgd_bind(mmp, &mmp->mm->pgd))
            panic("MM: fork: cannot bind new pgdir");
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
    /* free memory */
    if (clear_proc) {
        mmput(mmp->mm);
        mmp->mm = NULL;

        mmp->flags &= ~MMPF_INUSE;
    } else { /* clear mem regions only */
        if (atomic_get(&mmp->mm->refcnt) != 1) return EPERM;

        region_free_mm(mmp->mm);

        /* clear page table */
        pgd_free(&mmp->mm->pgd);
        if (pgd_new(&mmp->mm->pgd) != OK)
            panic("mm: proc_free failed to allocate page table");
        if (pgd_bind(mmp, &mmp->mm->pgd))
            panic("mm: proc_free failed to bind new page table");
    }

    return 0;
}
