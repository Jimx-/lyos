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

/*****************************************************************************
 *                                kill
 *************************************************************************//**
 * Send a signal to the process.
 * 
 * @param pid 	The process.
 * @param signo	The signal.
 *****************************************************************************/
PUBLIC int kill(int pid,int signo)
{
	MESSAGE msg;
	msg.type	= KILL;
	msg.PID		= pid;
	msg.SIGNR	= signo;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}

/*****************************************************************************
 *                                raise
 *************************************************************************//**
 * Send a signal to current process.
 * 
 * @param signo	The signal.
 *****************************************************************************/
PUBLIC int raise(int signo)
{
	MESSAGE msg;
	msg.type	= RAISE;
	msg.SIGNR	= signo;

	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.RETVAL;
}
