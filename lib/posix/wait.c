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
#include "stdio.h"
#include "assert.h"
#include "lyos/const.h"
#include "lyos/ipc.h"
#include "lyos/protect.h"
#include "lyos/proc.h"

/*****************************************************************************
 *                                wait
 *****************************************************************************/
/**
 * Wait for the child process to terminiate.
 * 
 * @param status  The value returned from the child.
 * 
 * @return  PID of the terminated child.
 *****************************************************************************/
PUBLIC int wait(int * status)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = -1;
	msg.SIGNR = 0;

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return (msg.PID == NO_TASK ? -1 : msg.PID);
}

/*****************************************************************************
 *                                waitpid
 *****************************************************************************/
/**
 * Wait for the child process to terminiate.
 * 
 * @param pid	  The pid of the child process to wait.
 * @param status  The value returned from the child.
 * @param options Options.
 * 
 * @return  PID of the terminated child.
 *****************************************************************************/
PUBLIC int waitpid(int pid, int * status, int options)
{
	MESSAGE msg;
	msg.type   = WAIT;

	msg.PID = pid;
	msg.SIGNR = options;

	send_recv(BOTH, TASK_MM, &msg);

	*status = msg.STATUS;

	return (msg.PID == NO_TASK ? -1 : msg.PID);
}
