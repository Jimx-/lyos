/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include "lyos/proc.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <sys/wait.h>
#include <lyos/vm.h>
#include "global.h"
#include "proto.h"

PRIVATE void check_parent(struct pmproc * pmp, int try_cleanup);

/**
 * @brief Perform the fork() syscall.
 * 
 * @param p Ptr to message.
 * @return Zero on success, otherwise the error code.
 */
PUBLIC int do_fork(MESSAGE * p)
{
    int child_slot = 0, n = 0;
    endpoint_t parent_ep = p->source, child_ep;

    if (procs_in_use >= NR_PROCS) return EAGAIN; /* proc table full */

    do {
        child_slot = (child_slot + 1) % (NR_PROCS);
        n++;
    } while((pmproc_table[child_slot].flags & PMPF_INUSE) && n <= NR_PROCS);

    if (n > NR_PROCS) return EAGAIN;

    /* tell MM */
    MESSAGE msg2mm;
    msg2mm.type = PM_MM_FORK;
    msg2mm.ENDPOINT = parent_ep;
    msg2mm.PROC_NR = child_slot;
    send_recv(BOTH, TASK_MM, &msg2mm);
    if (msg2mm.RETVAL != 0) return msg2mm.RETVAL;
    child_ep = msg2mm.ENDPOINT;

    struct pmproc * pmp = &pmproc_table[child_slot];
    pmp->flags = PMPF_INUSE;
    procs_in_use++;

    pmp->parent = parent_ep;
    pmp->endpoint = child_ep;
    pmp->pid = find_free_pid();

    p->PID = pmp->pid;

    /* tell FS, see fs_fork() */
    MESSAGE msg2fs;
    msg2fs.type = PM_VFS_FORK;
    msg2fs.ENDPOINT = child_slot;
    msg2fs.PENDPOINT = parent_ep;
    msg2fs.PID = pmp->pid;
    send_recv(BOTH, TASK_FS, &msg2fs);

    /* birth of the child */
    MESSAGE msg2child;
    msg2child.type = SYSCALL_RET;
    msg2child.RETVAL = 0;
    msg2child.PID = 0;
    send_recv(SEND, child_ep, &msg2child);

    return 0;
}

/*****************************************************************************
 *                                do_exit
 *****************************************************************************/
/**
 * Perform the exit() syscall.
 *
 * If proc A calls exit(), then PM will do the following in this routine:
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
PUBLIC int do_exit(MESSAGE * p)
{
    int i;
    endpoint_t src = p->source;
    int status = p->STATUS;
    int retval, src_slot, parent_slot;
    if ((retval = pm_verify_endpt(src, &src_slot)) != 0) return retval;
    struct pmproc * pmp = &pmproc_table[src_slot];

    int parent_ep = pmp->parent;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct pmproc * parent = &pmproc_table[parent_slot];

    kernel_clear(src);

    /* tell FS, see fs_exit() */
    MESSAGE msg2fs;
    msg2fs.type = EXIT;
    msg2fs.ENDPOINT = src;
    send_recv(BOTH, TASK_FS, &msg2fs);
    
    procctl(src, PCTL_CLEARPROC);
    
    pmp->exit_status = status;

    check_parent(pmp, 1);

    struct pmproc * pi = pmproc_table;
    /* if the proc has any child, make INIT the new parent */
    for (i = 0; i < NR_PROCS; i++, pi++) {
        if (pi->parent == src) { /* is a child */
            pi->parent = INIT;
            if ((pmproc_table[INIT].flags & PMPF_WAITING) &&
                (pi->flags & PMPF_HANGING)) {
                check_parent(pi, 1);
            }
        }
    }

    return SUSPEND;
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
PRIVATE void cleanup(struct pmproc * pmp)
{
    /* release the proc */
    procs_in_use--;
    pmp->pid = 0;
    pmp->flags = 0;
}

PRIVATE void tell_parent(struct pmproc * pmp)
{
    int retval;
    endpoint_t parent_ep = pmp->parent;
    int parent_slot;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return;
    struct pmproc * parent = &pmproc_table[parent_slot];
    parent->flags &= ~PMPF_WAITING;

    MESSAGE msg2parent;
    msg2parent.type = SYSCALL_RET;
    msg2parent.PID = pmp->pid;
    msg2parent.STATUS = pmp->exit_status;
    send_recv(SEND, pmp->parent, &msg2parent);

}

PRIVATE int waiting_for(struct pmproc * parent, struct pmproc * child)
{
    return (parent->flags & PMPF_WAITING) && (parent->wait_pid == -1 || parent->wait_pid == child->pid);

}

PRIVATE void check_parent(struct pmproc * pmp, int try_cleanup)
{
    int retval;
    endpoint_t parent_ep = pmp->parent;
    int parent_slot;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return;
    struct pmproc * parent = &pmproc_table[parent_slot];

    if (waiting_for(parent, pmp)) {
        tell_parent(pmp);
        if (try_cleanup) cleanup(pmp);
    } else {
        pmp->flags |= PMPF_HANGING;
        sig_proc(parent, SIGCHLD);
    }
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
PUBLIC int do_wait(MESSAGE * p)
{
    endpoint_t parent_ep = p->source;
    int parent_slot, retval;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct pmproc * parent = &pmproc_table[parent_slot]; 
    int child_pid = p->PID;
    /*
     * The value of child_pid can be:
     * (1) < -1     meaning waiting for any child process whose process group ID is
     *              equal to the absolute value of child_pid.
     * (2) -1       meaning waiting for any child process.
     * (3) 0        meaning wait for any child process whose process group ID is
     *              equal to that of the calling process.
     *
     * (4) > 0      meaning wait for the child whose process ID is equal to the
     *              value of child_pid.
     */
    int options = p->OPTIONS;

    int i;
    int children = 0;
    struct pmproc * pmp = pmproc_table;
    for (i = 0; i < NR_PROCS; i++, pmp++) {
        if (pmp->parent == parent_ep) {
            if (child_pid > 0 && child_pid != pmp->pid) continue;
            //if (child_pid < -1 && -child_pid != p_proc->gid) continue;
            //if (child_pid == 0 && p_proc->gid != (proc_table + pid)->gid) continue;
            children++;
            if (pmp->flags & PMPF_HANGING) {
                check_parent(pmp, 1);
                return SUSPEND;
            }
        }
    }

    if (children) {
        /* has children, but no child is HANGING */
        if (options & WNOHANG) return 0;   /* parent does not want to wait */
        parent->flags |= PMPF_WAITING;
        parent->wait_pid = child_pid;
        return SUSPEND;
    } else {
        /* no child at all */
        p->PID = -1;
        return ECHILD;
    }

    return SUSPEND;
}
