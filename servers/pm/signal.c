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
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <signal.h>
#include "errno.h"
#include "pmproc.h"
#include "proto.h"
#include "const.h"
#include "global.h"
 
PRIVATE int kill_sig(struct pmproc * pmp, pid_t dest, int signo);

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
              
    pmp->sigaction[signum].sa_handler = new_sa.sa_handler;
    pmp->sigaction[signum].sa_mask = new_sa.sa_mask;
    pmp->sigaction[signum].sa_flags = new_sa.sa_flags;
    pmp->sigreturn_f = (vir_bytes)p->SIGRET;
              
    return 0;
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
    si.sig_handler = p_dest->sigaction[signo].sa_handler;
    si.sig_return = p_dest->sigreturn_f;

    retval = kernel_sigsend(p_dest->endpoint, &si);

    return retval;
}

PRIVATE void sig_proc(struct pmproc * p_dest, int signo)
{
    send_sig(p_dest, signo);
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

        sig_proc(p_dest, signo);

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
