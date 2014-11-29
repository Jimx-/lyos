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

#define FE_DEBUG
#ifdef FE_DEBUG
#define DEB(x) printl("forkexit: "); x
#else
#define DEB(x)
#endif

PRIVATE void cleanup(struct proc * proc);

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
	/* find a free slot in proc_table */
	struct proc* p = &proc_table[child_slot];
	struct mmproc * mmp = &mmproc_table[child_slot];

	/* duplicate the process table */
	int parent_slot, retval;
	if ((retval = mm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
	struct mmproc * mmparent = mmproc_table + parent_slot;
	*p = proc_table[parent_slot];
	*mmp = mmproc_table[parent_slot];
	mmp->slot = child_slot;
	mmp->endpoint = child_slot;
	child_ep = child_slot;
	mmp->flags &= MMPF_INUSE;
	p->p_parent = parent_ep;
	sprintf(p->name, "%s_%d", proc_table[child_slot].name, child_ep);

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

/*****************************************************************************
 *                                do_exit
 *****************************************************************************/
/**
 * Perform the exit() syscall.
 *
 * If proc A calls exit(), then MM will do the following in this routine:
 *     <1> inform FS so that the fd-related things will be cleaned up
 *     <2> free A's memory
 *     <3> set A.exit_status, which is for the parent
 *     <4> depends on parent's status. if parent (say P) is:
 *           (1) WAITING
 *                 - clean P's WAITING bit, and
 *                 - send P a message to unblock it
 *                 - release A's proc_table[] slot
 *           (2) not WAITING
 *                 - set A's HANGING bit
 *     <5> iterate proc_table[], if proc B is found as A's child, then:
 *           (1) make INIT the new parent of B, and
 *           (2) if INIT is WAITING and B is HANGING, then:
 *                 - clean INIT's WAITING bit, and
 *                 - send INIT a message to unblock it
 *                 - release B's proc_table[] slot
 *               else
 *                 if INIT is WAITING but B is not HANGING, then
 *                     - B will call exit()
 *                 if B is HANGING but INIT is not WAITING, then
 *                     - INIT will call wait()
 *
 * TERMs:
 *     - HANGING: everything except the proc_table entry has been cleaned up.
 *     - WAITING: a proc has at least one child, and it is waiting for the
 *                child(ren) to exit()
 *     - zombie: say P has a child A, A will become a zombie if
 *         - A exit(), and
 *         - P does not wait(), neither does it exit(). that is to say, P just
 *           keeps running without terminating itself or its child
 * 
 * @param status  Exiting status for parent.
 * 
 *****************************************************************************/
PUBLIC void do_exit(int status)
{
	int i;
	int pid = mm_msg.source; /* PID of caller */
	int parent_pid = proc_table[pid].p_parent;
	struct proc * p = &proc_table[pid];
	struct mmproc * mmp = &mmproc_table[pid];

	/* tell FS, see fs_exit() */
	MESSAGE msg2fs;
	msg2fs.type = EXIT;
	msg2fs.PID = pid;
	send_recv(BOTH, TASK_FS, &msg2fs);
	
	proc_free(mmp);
	
	p->exit_status = status;

	if (proc_table[parent_pid].state & PST_WAITING) { /* parent is waiting */
		PST_UNSET(&proc_table[parent_pid], PST_WAITING);
		cleanup(&proc_table[pid]);
	}
	else { /* parent is not waiting */
		PST_SET(&proc_table[parent_pid], PST_HANGING);
	}

	/* if the proc has any child, make INIT the new parent */
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
		if (proc_table[i].p_parent == pid) { /* is a child */
			proc_table[i].p_parent = INIT;
			if ((proc_table[INIT].state & PST_WAITING) &&
			    (proc_table[i].state & PST_HANGING)) {
				PST_UNSET(&proc_table[INIT], PST_WAITING);
				cleanup(&proc_table[i]);
			}
		}
	}
}

/*****************************************************************************
 *                                cleanup
 *****************************************************************************/
/**
 * Do the last jobs to clean up a proc thoroughly:
 *     - Send proc's parent a message to unblock it, and
 *     - release proc's proc_table[] entry
 * 
 * @param proc  Process to clean up.
 *****************************************************************************/
PRIVATE void cleanup(struct proc * proc)
{
	MESSAGE msg2parent;
	msg2parent.type = SYSCALL_RET;
	msg2parent.PID = proc2pid(proc);
	msg2parent.STATUS = proc->exit_status;
	send_recv(SEND, proc->p_parent, &msg2parent);
	PST_SET(proc, PST_FREE_SLOT);
}

/*****************************************************************************
 *                                do_wait
 *****************************************************************************/
/**
 * Perform the wait() syscall.
 *
 * If proc P calls wait(), then MM will do the following in this routine:
 *     <1> iterate proc_table[],
 *         if proc A is found as P's child and it is HANGING
 *           - reply to P (cleanup() will send P a messageto unblock it)
 *           - release A's proc_table[] entry
 *           - return (MM will go on with the next message loop)
 *     <2> if no child of P is HANGING
 *           - set P's WAITING bit
 *     <3> if P has no child at all
 *           - reply to P with error
 *     <4> return (MM will go on with the next message loop)
 *
 *****************************************************************************/
PUBLIC void do_wait()
{
	int pid = mm_msg.source;
	int child_pid = mm_msg.PID;
	/*
	 * The value of child_pid can be:
	 * (1) < -1 	meaning waiting for any child process whose process group ID is
     *    			equal to the absolute value of child_pid.
	 * (2) -1 		meaning waiting for any child process.
	 * (3) 0		meaning wait for any child process whose process group ID is
     *    			equal to that of the calling process.
	 *
	 * (4) > 0  	meaning wait for the child whose process ID is equal to the
     *     			value of child_pid.
	 */
	int options = mm_msg.SIGNR;

	int i;
	int children = 0;
	struct proc* p_proc = proc_table;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++,p_proc++) {
		if (p_proc->p_parent == pid) {
			if (child_pid > 0 && child_pid != proc2pid(p_proc)) continue;
			if (child_pid < -1 && -child_pid != p_proc->gid) continue;
			if (child_pid == 0 && p_proc->gid != (proc_table + pid)->gid) continue;
			children++;
			if (p_proc->state & PST_HANGING) {
				cleanup(p_proc);
				return;
			}
		}
	}

	if (children) {
		/* has children, but no child is HANGING */
		if (options & WNOHANG) return;	 /* parent does not want to wait */
		proc_table[pid].state |= PST_WAITING;
	}
	else {
		/* no child at all */
		MESSAGE msg;
		msg.type = SYSCALL_RET;
		msg.PID = -1;
		send_recv(SEND, pid, &msg);
	}
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
