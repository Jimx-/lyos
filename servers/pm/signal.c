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
#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/sysutils.h>
#include <signal.h>
#include "errno.h"
#include "pmproc.h"
#include "proto.h"
#include "const.h"
#include "global.h"
 
PRIVATE int kill_sig(struct pmproc * pmp, pid_t dest, int signo);
PRIVATE void check_pending(struct pmproc * pmp);

PUBLIC int do_sigaction(MESSAGE * p)
{
    struct pmproc * pmp = pm_endpt_proc(p->source);
    struct sigaction new_sa;
    struct sigaction * save;
    struct sigaction * old_action = p->OLDSA;

    if (pmp == NULL) return EINVAL;
    
    int signum = p->SIGNR;

    if (signum == SIGKILL) return 0;
    if (signum < 1 || signum > NSIG) return -EINVAL;

    /* save the old action */
    save = &pmp->sigaction[signum];
    if(old_action) {
        data_copy(p->source, old_action, SELF, save, sizeof(struct sigaction));
    }
    
    if (!p->NEWSA) return 0;
    data_copy(SELF, &new_sa, p->source, p->NEWSA, sizeof(struct sigaction));
              
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
    pmp->sigreturn_f = (vir_bytes)p->SIGRET;
              
    return 0;
}

PUBLIC int do_sigprocmask(MESSAGE * p)
{
    struct pmproc * pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    sigset_t set;
    int i;

    set = p->MASK;
    p->MASK = pmp->sig_mask;

    if (!set) return 0;

    switch (p->HOW) {
        case SIG_BLOCK:
            sigdelset(&set, SIGKILL);
            sigdelset(&set, SIGSTOP);
            for (i = 1; i < NSIG; i++) {
                if (sigismember(&set, i))
                sigaddset(&pmp->sig_mask, i);
            }
            break;
        case SIG_UNBLOCK:
            for (i = 1; i < NSIG; i++) {
            if (sigismember(&set, i))
                sigdelset(&pmp->sig_mask, i);
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

PUBLIC int do_sigsuspend(MESSAGE * p)
{
    struct pmproc * pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    pmp->sig_mask_saved = pmp->sig_mask;
    pmp->sig_mask = p->MASK;
    sigdelset(&pmp->sig_mask, SIGKILL);
    sigdelset(&pmp->sig_mask, SIGSTOP);
    pmp->flags &= PMPF_SIGSUSPENDED;

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
PUBLIC int do_kill(MESSAGE * p)
{
    struct pmproc * pmp = pm_endpt_proc(p->source);
    if (pmp == NULL) return EINVAL;

    int sig = p->SIGNR;
    int pid = p->PID; 
    return kill_sig(pmp, pid, sig);
}

PRIVATE int send_sig(struct pmproc * p_dest, int signo)
{
    struct siginfo si;
    int retval;
        
    si.signo = signo;
    si.sig_handler = (vir_bytes) p_dest->sigaction[signo].sa_handler;
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

PUBLIC void sig_proc(struct pmproc * p_dest, int signo, int trace)
{
    /* signal the tracer first */
    if (trace && p_dest->tracer != NO_TASK && signo != SIGKILL) {
        sigaddset(&p_dest->sig_trace, signo);

        if (!(p_dest->flags & PMPF_TRACED)) trace_signal(p_dest, signo);

        return;
    }

    int bad_ignore = sigismember(&noign_set, signo) && (
        sigismember(&p_dest->sig_ignore, signo) ||
        sigismember(&p_dest->sig_mask, signo));

    /* the signal is ignored */
    if (!bad_ignore && sigismember(&p_dest->sig_ignore, signo)) return;
    /* the signal is blocked */
    if (!bad_ignore && sigismember(&p_dest->sig_mask, signo)) {
        sigaddset(&p_dest->sig_pending, signo);
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

PRIVATE int kill_sig(struct pmproc * pmp, pid_t dest, int signo)
{
    int errcode = ESRCH;
    int count = 0;

    if (signo < 0 || signo >= NSIG) return EINVAL;

    if (dest == INIT_PID && signo == SIGKILL) return EINVAL;    /* attempt to kill INIT */

    struct pmproc * p_dest;
    
    for (p_dest = &pmproc_table[NR_PROCS-1]; p_dest >= &pmproc_table[0]; p_dest--) {
        if (!(p_dest->flags & PMPF_INUSE)) continue;

        if (dest > 0 && dest != p_dest->pid) continue;
        if (dest == -1 && dest <= INIT) continue;
        if (dest == 0 && p_dest->procgrp != pmp->procgrp) continue;
        if (dest < -1 && p_dest->procgrp != -dest) continue;

        /* check permission */
        if (pmp->effuid != SU_UID &&
                pmp->realuid != p_dest->realuid &&
                pmp->effuid != p_dest->effuid &&
                pmp->realuid != p_dest->effuid &&
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

    if (count > 0) return 0;
    else return errcode;
}

PUBLIC int do_sigreturn(MESSAGE * p)
{
    int retval;

    retval = kernel_sigreturn(p->source, p->BUF);

    return retval;
}

PRIVATE void check_pending(struct pmproc * pmp)
{
    int i;

    for (i = 0; i < NSIG; i++) {
        if (sigismember(&pmp->sig_pending, i) && !sigismember(&pmp->sig_mask, i)) {
            sigdelset(&pmp->sig_pending, i);
            sig_proc(pmp, i, FALSE);
        }
    }
}

PUBLIC int process_ksig(endpoint_t target, int signo)
{
    struct pmproc * pmp = pm_endpt_proc(target);
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
