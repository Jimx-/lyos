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
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "signal.h"
#include "errno.h"
#include "region.h"
#include "proto.h"
 
 PRIVATE void sig_proc(struct proc * p_dest, int signo);

/*************************************************************************************
 * 						do_sigaction												 *
 * ***********************************************************************************/
 /**																				*				
  * 																				*
  *  Perform the sigaction syscall  												*
  * 																				*
  * *********************************************************************************/
PUBLIC int do_sigaction()
{
	struct proc * p = proc_table + mm_msg.source;
	struct sigaction new_sa;
	struct sigaction * save;
	struct sigaction * old_action = mm_msg.OLDSA;
	
	int signum = mm_msg.SIGNR;

	if (signum == SIGKILL) return 0;
	if (signum < 1 || signum > 32) return -EINVAL;

	/* save the old action */
	save = &p->sigaction[signum];
	if(old_action) {
		data_copy(mm_msg.source, old_action, TASK_MM, save, sizeof(struct sigaction));
	}
	
	if (!mm_msg.NEWSA) return 0;
	data_copy(TASK_MM, &new_sa, mm_msg.source, mm_msg.NEWSA, sizeof(struct sigaction));
			  
	p->sigaction[signum].sa_handler = new_sa.sa_handler;
	p->sigaction[signum].sa_mask = new_sa.sa_mask;
	p->sigaction[signum].sa_flags = new_sa.sa_flags;
			  
	return 0;
}

/*****************************************************************************
 *                                do_alarm
 *****************************************************************************/
/**
 *  Perform the alarm syscall
 *
 *****************************************************************************/
 PUBLIC int do_alarm()
{
	struct proc * p = proc_table + mm_msg.source;
	int seconds = mm_msg.SECONDS;
	int old = p->alarm;

	if (old)
		old = (old - jiffies) / 100;
	p->alarm = (seconds>0)?(jiffies+100*seconds):0;
	return (old);
}

/*****************************************************************************
 *                                send_sig
 *****************************************************************************/
/**
 *  Send a signal to the process given by p.
 *
 *****************************************************************************/
PUBLIC int send_sig(struct proc * p, int sig)
{
	return 0;
}

/*****************************************************************************
 *                                do_kill
 *****************************************************************************/
/**
 *  Send a signal to the process given by pid.
 *
 * @param PID		u.m3.m3i2 the pid of the process.
 * @param SIGNR		u.m3.m3i1 the signal.
 *****************************************************************************/
PUBLIC int do_kill()
{
	int sig = mm_msg.SIGNR;
	int pid = mm_msg.PID;
	
	return kill_sig(mm_msg.source, pid, sig);
}

PUBLIC int kill_sig(pid_t source, pid_t dest, int signo)
{
	struct proc * p_src = proc_table + source;
	int errcode = ESRCH;
	int count = 0;

	if (signo < 0 || signo >= NSIG) return EINVAL;

	if (dest == INIT && signo == SIGKILL) return EINVAL;	/* attempt to kill INIT */

	struct proc* p_dest = proc_table + NR_PROCS;
    
    while (--p_dest > &FIRST_PROC) {
    	if (p_dest->state == PST_FREE_SLOT) continue;

    	if (dest > 0 && dest != proc2pid(p_dest)) continue;
    	if (dest == -1 && dest <= INIT) continue;

    	/* check permission */
    	if (p_src->euid != SU_UID &&
    			p_src->uid != p_dest->uid &&
    			p_src->euid != p_dest->euid &&
    			p_src->uid != p_dest->euid &&
    			p_src->euid != p_dest->uid) {
    		errcode = EPERM;
    		continue;
    	}

    	count++;

		sig_proc(p_dest, signo);

		if (dest > 0) break;
	}

	/* the process has killed itself and thus no longer waiting */
	if (p_src->state != PST_RECEIVING) return SUSPEND;

	if (count > 0) return 0;
	else return errcode;
}

PRIVATE void sig_proc(struct proc * p_dest, int signo)
{
	if (!send_sig(p_dest, signo)) return;
}
