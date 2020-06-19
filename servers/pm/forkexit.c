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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include <sched.h>
#include "lyos/proc.h"
#include <lyos/sysutils.h>
#include <sys/wait.h>
#include <lyos/vm.h>
#include "global.h"
#include "proto.h"

static void check_parent(struct pmproc* pmp, int try_cleanup);

/**
 * @brief Perform the fork() syscall.
 *
 * @param p Ptr to message.
 * @return Zero on success, otherwise the error code.
 */
int do_fork(MESSAGE* p)
{
    int child_slot = 0, n = 0;
    void* newsp = p->BUF;
    int flags = p->FLAGS;
    endpoint_t parent_ep = p->source, child_ep;
    struct pmproc* pm_parent = pm_endpt_proc(parent_ep);
    if (!pm_parent) return EINVAL;

    if (procs_in_use >= NR_PROCS) return EAGAIN; /* proc table full */

    do {
        child_slot = (child_slot + 1) % (NR_PROCS);
        n++;
    } while ((pmproc_table[child_slot].flags & PMPF_INUSE) && n <= NR_PROCS);

    if (n > NR_PROCS) return EAGAIN;

    /* tell MM */
    MESSAGE msg2mm;
    msg2mm.type = PM_MM_FORK;
    msg2mm.ENDPOINT = parent_ep;
    msg2mm.PROC_NR = child_slot;
    msg2mm.BUF = newsp;
    msg2mm.FLAGS = flags;
    send_recv(BOTH, TASK_MM, &msg2mm);
    if (msg2mm.RETVAL != 0) return msg2mm.RETVAL;
    child_ep = msg2mm.ENDPOINT;

    struct pmproc* pmp = &pmproc_table[child_slot];
    *pmp = *pm_parent;
    pmp->flags = PMPF_INUSE;
    procs_in_use++;

    pmp->parent = parent_ep;
    pmp->endpoint = child_ep;
    pmp->pid = find_free_pid();

    /* thread-related */
    pmp->tgid = pmp->pid;
    if (flags & CLONE_THREAD) {
        pmp->tgid = pm_parent->tgid;
    }

    pmp->group_leader = pmp;
    INIT_LIST_HEAD(&pmp->thread_group);
    if (flags & CLONE_THREAD) {
        pmp->group_leader = pm_parent->group_leader;
        list_add(&pmp->thread_group, &pm_parent->group_leader->thread_group);
    }

    p->PID = pmp->pid;

    /* tell FS, see fs_fork() */
    if (parent_ep != TASK_FS) {
        MESSAGE msg2fs;
        msg2fs.type = PM_VFS_FORK;
        msg2fs.ENDPOINT = child_ep;
        msg2fs.PENDPOINT = parent_ep;
        msg2fs.PID = pmp->pid;
        send_recv(BOTH, TASK_FS, &msg2fs);
    }

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
int do_exit(MESSAGE* p)
{
    endpoint_t src = p->source;
    int status = p->STATUS;

    int retval, src_slot;
    if ((retval = pm_verify_endpt(src, &src_slot)) != 0) return retval;
    struct pmproc* pmp = &pmproc_table[src_slot];

    status = W_EXITCODE(status, 0);
    exit_proc(pmp, status);

    return SUSPEND;
}

void exit_proc(struct pmproc* pmp, int status)
{
    int i;
    endpoint_t ep = pmp->endpoint;

    kernel_clear(ep);

    /* tell FS, see fs_exit() */
    MESSAGE msg2fs;
    msg2fs.type = EXIT;
    msg2fs.ENDPOINT = ep;
    send_recv(BOTH, TASK_FS, &msg2fs);

    procctl(ep, PCTL_CLEARPROC);

    pmp->exit_status = status;

    check_parent(pmp, 1);

    struct pmproc* pi = pmproc_table;
    /* if the proc has any child, make INIT the new parent */
    for (i = 0; i < NR_PROCS; i++, pi++) {
        if (pi->parent == ep) { /* is a child */
            pi->parent = INIT;
            if ((pmproc_table[INIT].flags & PMPF_WAITING) &&
                (pi->flags & PMPF_HANGING)) {
                check_parent(pi, 1);
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
static void cleanup(struct pmproc* pmp)
{
    /* release the proc */
    procs_in_use--;
    pmp->pid = 0;
    pmp->flags = 0;
}

static void tell_parent(struct pmproc* pmp)
{
    int retval;
    endpoint_t parent_ep = pmp->parent;
    int parent_slot;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return;
    struct pmproc* parent = &pmproc_table[parent_slot];
    parent->flags &= ~PMPF_WAITING;

    MESSAGE msg2parent;
    msg2parent.type = SYSCALL_RET;
    msg2parent.PID = pmp->pid;
    msg2parent.STATUS = W_EXITCODE(pmp->exit_status, pmp->sig_status);
    send_recv(SEND_NONBLOCK, pmp->parent, &msg2parent);
}

int waiting_for(struct pmproc* parent, struct pmproc* child)
{
    return (parent->flags & PMPF_WAITING) &&
           (parent->wait_pid == -1 || parent->wait_pid == child->pid);
}

static void check_parent(struct pmproc* pmp, int try_cleanup)
{
    int retval;
    endpoint_t parent_ep = pmp->parent;
    int parent_slot;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return;
    struct pmproc* parent = &pmproc_table[parent_slot];
    if (waiting_for(parent, pmp)) {
        tell_parent(pmp);
        if (try_cleanup) cleanup(pmp);
    } else {
        pmp->flags |= PMPF_HANGING;
        sig_proc(parent, SIGCHLD, TRUE);
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
int do_wait(MESSAGE* p)
{
    endpoint_t parent_ep = p->source;
    int parent_slot, retval;
    if ((retval = pm_verify_endpt(parent_ep, &parent_slot)) != 0) return retval;
    struct pmproc* parent = &pmproc_table[parent_slot];
    int child_pid = p->PID;
    /*
     * The value of child_pid can be:
     * (1) < -1     meaning waiting for any child process whose process group ID
     * is equal to the absolute value of child_pid. (2) -1       meaning waiting
     * for any child process. (3) 0        meaning wait for any child process
     * whose process group ID is equal to that of the calling process.
     *
     * (4) > 0      meaning wait for the child whose process ID is equal to the
     *              value of child_pid.
     */
    int options = p->OPTIONS;

    int i;
    int children = 0;
    struct pmproc* pmp = pmproc_table;
    for (i = 0; i < NR_PROCS; i++, pmp++) {
        if (pmp->parent == parent_ep || pmp->tracer == parent_ep) {
            if ((pmp->flags & PMPF_INUSE) == 0) continue;

            /* select the right ones */
            if (child_pid > 0 && child_pid != pmp->pid) continue;
            if (child_pid < -1 && -child_pid != pmp->procgrp) continue;
            if (child_pid == 0 && parent->procgrp != pmp->procgrp) continue;

            children++;
            if (pmp->tracer == parent_ep) {
                MESSAGE msg2tracer;

                if (pmp->flags & PMPF_TRACED) {
                    int i;
                    for (i = 1; i < NSIG; i++) {
                        if (sigismember(&pmp->sig_trace, i)) {
                            sigdelset(&pmp->sig_trace, i);

                            msg2tracer.type = SYSCALL_RET;
                            msg2tracer.PID = pmp->pid;
                            msg2tracer.STATUS = W_STOPCODE(i);
                            send_recv(SEND_NONBLOCK, parent_ep, &msg2tracer);
                            return SUSPEND;
                        }
                    }
                }
            }

            if (pmp->flags & PMPF_HANGING) {
                parent->flags |= PMPF_WAITING;
                parent->wait_pid = child_pid;
                check_parent(pmp, 1);
                return SUSPEND;
            }
        }
    }

    if (children) {
        /* has children, but no child is HANGING */
        if (options & WNOHANG) return 0; /* parent does not want to wait */
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
