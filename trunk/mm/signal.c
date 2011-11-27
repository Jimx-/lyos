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
    
#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "signal.h"
#include "errno.h"

PRIVATE int send_sig(int sig,struct proc * p);
 
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
	struct sigaction new_sa;
	struct sigaction * save;
	struct sigaction * old_action = mm_msg.OLDSA;
	
	int signum = mm_msg.SIGNR;

	if (signum < 1 || signum > 32) return -EINVAL;
	if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL;

	save = &current->sigaction[signum];
	if((struct sigaction *)old_action)
		phys_copy(va2la(mm_msg.source, old_action),
				  va2la(TASK_MM, save),
				  sizeof(struct sigaction));
	
	if (!mm_msg.NEWSA) return 0;
	phys_copy(va2la(TASK_MM, &new_sa),
			  va2la(mm_msg.source, mm_msg.NEWSA),
			  sizeof(struct sigaction));
			  
	current->sigaction[signum].sa_handler = new_sa.sa_handler;
	current->sigaction[signum].sa_mask = new_sa.sa_mask;
	current->sigaction[signum].sa_flags = new_sa.sa_flags;
			  
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
	int seconds = mm_msg.SECONDS;
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / 100;
	current->alarm = (seconds>0)?(jiffies+100*seconds):0;
	return (old);
}

/*****************************************************************************
 *                                send_sig
 *****************************************************************************/
/**
 *  Send a signal to the process given by p.
 *
 *****************************************************************************/
 PRIVATE int send_sig(int sig,struct proc * p)
{
	if (!p || sig<1 || sig>32)
		return -EINVAL;

		/* send the signal */
		p->signal |= (1<<(sig-1));
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
	struct proc* p_dest = proc_table + pid;
	int ret; 
	
	if (pid == -1){
	    p_dest = proc_table + NR_PROCS;
        while (--p_dest > &FIRST_PROC)
		send_sig(sig,p_dest);
        return 0;
	}

	ret = send_sig(sig,p_dest);
	return ret;
}

PUBLIC int do_raise()
{
	int sig = mm_msg.SIGNR;
	
	return send_sig(sig, proc_table + getpid());
}
