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

#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/sysutils.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "errno.h"
#include "pmproc.h"
#include "proto.h"
#include "const.h"
#include "global.h"

static int kill_sig(struct pmproc* pmp, pid_t dest, int signo);
static void check_pending(struct pmproc* pmp);
static void signalfd_poll_notify(endpoint_t endpoint, int status);

int do_sigaction(MESSAGE* p)
{
    struct pmproc* pmp = pm_endpt_proc(p->source);
    struct sigaction new_sa;
    struct sigaction* save;
    struct sigaction* old_action = p->u.m_pm_signal.oldact;

    if (pmp == NULL) return EINVAL;

    int signum = p->u.m_pm_signal.signum;

    if (signum == SIGKILL) return 0;
    if (signum < 1 || signum > NSIG) return -EINVAL;

    /* save the old action */
    save = &pmp->sigaction[signum];
    if (old_action) {
        data_copy(p->source, old_action, SELF, save, sizeof(struct sigaction));
    }

    if (!p->u.m_pm_signal.act) return 0;
    data_copy(SELF, &new_sa, p->source, p->u.m_pm_signal.act,
              sizeof(struct sigaction));

    if (new_sa.sa_handler == SIG_IGN) {
        sigaddset(&pmp->sig_ignore, signum);
        sigdelset(&pmp->sig_pending, signum);
        sigdelset(&pmp->sig_catch, signum);
    } else if (new_sa.sa_handler == SIG_DFL) {
        sigdelset(&pmp->sig_ignore, signum);
        sigdelset(&pmp->sig_catch, signum);
    } else {
        sigdelset(&pmp->sig_ignore, signum);
        sigaddset(&pmp->sig_catch, signum);
    }

    pmp->sigaction[signum].sa_handler = new_sa.sa_handler;
    sigdelset(&new_sa.sa_mask, SIGKILL);
    sigdelset(&new_sa.sa_mask, SIGSTOP);
    pmp->sigaction[signum].sa_mask = new_sa.sa_mask;
    pmp->sigaction[signum].sa_flags = new_sa.sa_flags;
    pmp->sigreturn_f = p->u.m_pm_signal.sigret;

    return 0;
}

int do_sigprocmask(MESSAGE* p)
{
    sigset_t set;
    int i;

    struct pmproc* pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    set = p->MASK;
    p->MASK = pmp->sig_mask;

    if (!set) return 0;

    switch (p->HOW) {
    case SIG_BLOCK:
        sigdelset(&set, SIGKILL);
        sigdelset(&set, SIGSTOP);
        for (i = 1; i < NSIG; i++) {
            if (sigismember(&set, i)) sigaddset(&pmp->sig_mask, i);
        }
        break;
    case SIG_UNBLOCK:
        for (i = 1; i < NSIG; i++) {
            if (sigismember(&set, i)) sigdelset(&pmp->sig_mask, i);
        }
        check_pending(pmp);
        break;
    case SIG_SETMASK:
        sigdelset(&set, SIGKILL);
        sigdelset(&set, SIGSTOP);
        pmp->sig_mask = set;
        break;
    default:
        return EINVAL;
    }
    return 0;
}

int do_sigsuspend(MESSAGE* p)
{
    struct pmproc* pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    pmp->sig_mask_saved = pmp->sig_mask;
    pmp->sig_mask = p->MASK;
    sigdelset(&pmp->sig_mask, SIGKILL);
    sigdelset(&pmp->sig_mask, SIGSTOP);
    pmp->flags |= PMPF_SIGSUSPENDED;

    check_pending(pmp);
    return SUSPEND;
}

/*****************************************************************************
 *                                do_kill
 *****************************************************************************/
/**
 *  Send a signal to the process given by pid.
 *
 * @param PID       u.m3.m3i2 the pid of the process.
 * @param SIGNR     u.m3.m3i1 the signal.
 *****************************************************************************/
int do_kill(MESSAGE* p)
{
    struct pmproc* pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    int sig = p->SIGNR;
    int pid = p->PID;

    return kill_sig(pmp, pid, sig);
}

static int send_sig(struct pmproc* p_dest, int signo)
{
    struct siginfo si;
    int retval;

    si.signo = signo;

    if (p_dest->flags & PMPF_SIGSUSPENDED) {
        si.mask = p_dest->sig_mask_saved;
    } else {
        si.mask = p_dest->sig_mask;
    }

    si.sig_handler = p_dest->sigaction[signo].sa_handler;
    si.sig_return = p_dest->sigreturn_f;

    /*if (p_dest->sigaction[signo].sa_flags & SA_NODEFER)
        sigdelset(&p_dest->sig_mask, signo);
    else
        sigaddset(&p_dest->sig_mask, signo);*/

    int i;
    for (i = 0; i < NSIG; i++) {
        if (sigismember(&p_dest->sigaction[signo].sa_mask, i))
            sigaddset(&p_dest->sig_mask, i);
    }

    /*if (p_dest->sigaction[signo].sa_flags & SA_RESETHAND) {
        sigdelset(&p_dest->sig_catch, signo);
        p_dest->sigaction[signo].sa_handler = SIG_DFL;
    }*/

    retval = kernel_sigsend(p_dest->endpoint, &si);

    /* unblock the proc if it's suspended */
    if (p_dest->flags & PMPF_SIGSUSPENDED) {
        p_dest->flags &= ~PMPF_SIGSUSPENDED;

        MESSAGE msg;
        msg.type = SYSCALL_RET;
        msg.RETVAL = EINTR;
        send_recv(SEND, p_dest->endpoint, &msg);
    }

    return retval;
}

void sig_proc(struct pmproc* p_dest, int signo, int trace)
{
    /* signal the tracer first */
    if (trace && p_dest->tracer != NO_TASK && signo != SIGKILL) {
        sigaddset(&p_dest->sig_trace, signo);

        if (!(p_dest->flags & PMPF_TRACED)) trace_signal(p_dest, signo);

        return;
    }

    int bad_ignore = sigismember(&noign_set, signo) &&
                     (sigismember(&p_dest->sig_ignore, signo) ||
                      sigismember(&p_dest->sig_mask, signo));

    /* the signal is ignored */
    if (!bad_ignore && sigismember(&p_dest->sig_ignore, signo)) return;
    /* the signal is blocked */
    if (!bad_ignore && sigismember(&p_dest->sig_mask, signo)) {
        sigaddset(&p_dest->sig_pending, signo);

        /* notify vfs */
        if (sigismember(&p_dest->sig_poll, signo))
            signalfd_poll_notify(p_dest->endpoint, signo);

        return;
    }

    if (!bad_ignore && sigismember(&p_dest->sig_catch, signo)) {
        if (send_sig(p_dest, signo) == 0) return;
    } else if (!bad_ignore && sigismember(&ign_set, signo)) {
        return;
    }

    p_dest->sig_status = signo;
    exit_proc(p_dest, 0);
}

static int kill_sig(struct pmproc* pmp, pid_t dest, int signo)
{
    int errcode = ESRCH;
    int count = 0;

    if (signo < 0 || signo >= NSIG) return EINVAL;

    if (dest == INIT_PID && signo == SIGKILL)
        return EINVAL; /* attempt to kill INIT */

    struct pmproc* p_dest;

    for (p_dest = &pmproc_table[NR_PROCS - 1]; p_dest >= &pmproc_table[0];
         p_dest--) {
        if (!(p_dest->flags & PMPF_INUSE)) continue;

        if (dest > 0 && dest != p_dest->pid) continue;
        if (dest == -1 && dest <= INIT) continue;
        if (dest == 0 && p_dest->procgrp != pmp->procgrp) continue;
        if (dest < -1 && p_dest->procgrp != -dest) continue;

        /* check permission */
        if (pmp->effuid != SU_UID && pmp->realuid != p_dest->realuid &&
            pmp->effuid != p_dest->effuid && pmp->realuid != p_dest->effuid &&
            pmp->effuid != p_dest->realuid) {
            errcode = EPERM;
            continue;
        }

        count++;

        sig_proc(p_dest, signo, TRUE);

        if (dest > 0) break;
    }

    /* the process has killed itself and thus no longer waiting */
    if (!(pmp->flags & PMPF_INUSE)) return SUSPEND;

    if (count > 0)
        return 0;
    else
        return errcode;
}

int do_sigreturn(MESSAGE* p)
{
    struct pmproc* pmp = pm_endpt_proc(p->source);
    int retval;

    if (pmp == NULL) return EINVAL;

    pmp->sig_mask = p->MASK;
    sigdelset(&pmp->sig_mask, SIGKILL);
    sigdelset(&pmp->sig_mask, SIGSTOP);

    retval = kernel_sigreturn(p->source, p->BUF);

    return retval;
}

static void check_pending(struct pmproc* pmp)
{
    int i;

    for (i = 0; i < NSIG; i++) {
        if (sigismember(&pmp->sig_pending, i) &&
            !sigismember(&pmp->sig_mask, i)) {
            sigdelset(&pmp->sig_pending, i);
            sig_proc(pmp, i, FALSE);
        }
    }
}

int process_ksig(endpoint_t target, int signo)
{
    struct pmproc* pmp = pm_endpt_proc(target);
    if (!pmp) return EINVAL;

    if (!(pmp->flags & PMPF_INUSE)) return EINVAL;

    pid_t pid = pmp->pid;

    switch (signo) {
    case SIGINT:
        pid = 0;
        break;
    default:
        break;
    }

    kill_sig(pmp, pid, signo);

    if (!(pmp->flags & PMPF_INUSE)) return EINVAL;

    return 0;
}

static void signalfd_reply(endpoint_t endpoint, int status)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = PM_SIGNALFD_REPLY;
    msg.u.m_pm_vfs_signalfd_reply.endpoint = endpoint;
    msg.u.m_pm_vfs_signalfd_reply.status = status;

    send_recv(SEND, TASK_FS, &msg);
}

static void signalfd_poll_notify(endpoint_t endpoint, int status)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = PM_SIGNALFD_POLL_NOTIFY;
    msg.u.m_pm_vfs_signalfd_reply.endpoint = endpoint;
    msg.u.m_pm_vfs_signalfd_reply.status = status;

    send_recv(SEND, TASK_FS, &msg);
}

int do_signalfd_dequeue(MESSAGE* msg)
{
    int endpoint = msg->u.m_vfs_pm_signalfd.endpoint;
    sigset_t sigmask = (sigset_t)msg->u.m_vfs_pm_signalfd.sigmask;
    void* buf = msg->u.m_vfs_pm_signalfd.buf;
    int notify = msg->u.m_vfs_pm_signalfd.notify;
    struct pmproc* pmp = pm_endpt_proc(endpoint);
    struct signalfd_siginfo new;
    int retval, i, signo = 0;

    if (msg->source != TASK_FS) {
        return -EPERM;
    }

    if (!pmp) {
        retval = -EINVAL;
        goto reply;
    }

    for (i = 0; i < NSIG; i++) {
        if (sigismember(&pmp->sig_pending, i) && sigismember(&sigmask, i)) {
            sigdelset(&pmp->sig_pending, i);
            signo = i;
            break;
        }
    }

    if (signo) {
        memset(&new, 0, sizeof(new));
        new.ssi_signo = signo;

        retval = data_copy(endpoint, buf, SELF, &new, sizeof(new));
        if (retval) {
            retval = -retval;
            goto reply;
        }
    } else if (notify) {
        /* add signals to signalfd poll set */
        for (i = 0; i < NSIG; i++) {
            if (sigismember(&sigmask, i)) sigaddset(&pmp->sig_poll, i);
        }
    }

    retval = signo;

reply:
    signalfd_reply(endpoint, retval);

    return SUSPEND;
}

int do_signalfd_getnext(MESSAGE* msg)
{
    int endpoint = msg->u.m_vfs_pm_signalfd.endpoint;
    sigset_t sigmask = (sigset_t)msg->u.m_vfs_pm_signalfd.sigmask;
    int notify = msg->u.m_vfs_pm_signalfd.notify;
    struct pmproc* pmp = pm_endpt_proc(endpoint);
    int retval, i;

    if (msg->source != TASK_FS) {
        return -EPERM;
    }

    if (!pmp) {
        retval = -EINVAL;
        goto reply;
    }

    retval = 0;
    for (i = 0; i < NSIG; i++) {
        if (sigismember(&pmp->sig_pending, i) && sigismember(&sigmask, i)) {
            retval = i;
            break;
        }
    }

    if (retval == 0 && notify) {
        /* add signals to signalfd poll set */
        for (i = 0; i < NSIG; i++) {
            if (sigismember(&sigmask, i)) sigaddset(&pmp->sig_poll, i);
        }
    }

reply:
    signalfd_reply(endpoint, retval);

    return SUSPEND;
}
