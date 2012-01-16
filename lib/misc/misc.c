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
#include "assert.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

/*****************************************************************************
 *                                send_recv
 *****************************************************************************/
/**
 * <Ring 1~3> IPC syscall.
 *
 * It is an encapsulation of `sendrec',
 * invoking `sendrec' directly should be avoided
 *
 * @param function  SEND, RECEIVE or BOTH
 * @param src_dest  The caller's proc_nr
 * @param msg       Pointer to the MESSAGE struct
 * 
 * @return always 0.
 *****************************************************************************/
PUBLIC int send_recv(int function, int src_dest, MESSAGE* msg)
{
	int ret = 0;

	if (function == RECEIVE)
		memset(msg, 0, sizeof(MESSAGE));

	switch (function) {
	case BOTH:
		ret = sendrec(SEND, src_dest, msg);
		if (ret == 0)
			ret = sendrec(RECEIVE, src_dest, msg);
		break;
	case SEND:
	case RECEIVE:
		ret = sendrec(function, src_dest, msg);
		break;
	default:
		assert((function == BOTH) ||
		       (function == SEND) || (function == RECEIVE));
		break;
	}

	return ret;
}

/*****************************************************************************
 *                                spin
 *****************************************************************************/
PUBLIC void spin(char * func_name)
{
	printl("\nspinning in %s ...\n", func_name);
	while (1) {}
}


