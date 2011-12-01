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
#include "config.h"
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

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("FS: "); x
#else
#define DEB(x)
#endif

PUBLIC void task_fs()
{
/*
	int fs_nr = TASK_LYOS_FS; 

	while (1) {
		send_recv(RECEIVE, ANY, &fs_msg);
		int src = fs_msg.source;
		DEB(printl("Received a message, from %d\n", src));
		DEB(printl("Send the message to %d\n", fs_nr));
		send_recv(BOTH, fs_nr, &fs_msg);
		DEB(printl("Received message from fs driver.\n"));

		if (fs_msg.type != SUSPEND_PROC) {
			fs_msg.type = SYSCALL_RET;
			send_recv(SEND, src, &fs_msg);
		}
	}
*/
}
