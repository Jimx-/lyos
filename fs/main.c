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
#include "lyos/config.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <elf.h>
//#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

PUBLIC void task_fs()
{
	printl("VFS: VFS is running.\n");

	MESSAGE m;
	MESSAGE msg;

	while (1) {
		send_recv(RECEIVE, ANY, &m);

		if (m.type == RESUME_PROC) {
			send_recv(SEND, m.PROC_NR, &msg); 
			continue;
		}
		int src = m.source;
		pcaller = &proc_table[src];
		int msgtype = m.type;

		msg.type = VFS_REQUEST;
		msg.BUF = &m;
		send_recv(BOTH, TASK_LYOS_FS, &msg);
		if (msg.type != SUSPEND_PROC)
			send_recv(SEND, src, &msg);
	}

	/*struct file_system * fs;
	while (1){
		send_recv(RECEIVE, ANY, &msg);
		int src = msg.source;
		pcaller = &proc_table[src];
		int msgtype = msg.type;

		fs = file_systems;
		switch (msgtype) {
	 	case FS_REGISTER:
			register_filesystem(&msg);
			break;
		case OPEN:
			msg.FD = fs->open(&msg);
			break;
		case CLOSE:
			msg.RETVAL = fs->close(&msg);
			break;
		case READ:
		case WRITE:
			msg.CNT = fs->rdwt(&msg);
			break;
		case UNLINK:
			msg.RETVAL = fs->unlink(&msg);
			break;
		case MOUNT:
			msg.RETVAL = fs->mount(&msg);
			break;
		case UMOUNT:
			msg.RETVAL = fs->umount(&msg);
			break;
		case MKDIR:
			msg.RETVAL = fs->mkdir(&msg);
			break;
		case RESUME_PROC:
			src = msg.PROC_NR;
			break;
		case FORK:
			msg.RETVAL = fs->fork(&msg);
			break;
		case EXIT:
			msg.RETVAL = fs->exit(&msg);
			break;
		case LSEEK:
			msg.OFFSET = fs->lseek(&msg);
			break;
		case STAT:
		case FSTAT:
			msg.RETVAL = fs->stat(&msg);
			break;
		case CHROOT:
			msg.RETVAL = fs->chroot(&msg);
			break;
		case CHDIR:
			msg.RETVAL = fs->chdir(&msg);
			break; 
		default:
			dump_msg("VFS: Unknown message:", &msg);
			assert(0);
			break;
		}

		if (msg.type != SUSPEND_PROC) {
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
		}
	} */
}

