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

//#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

PRIVATE void dump_fs(struct file_system * fs);

PUBLIC void task_fs()
{

	init_buffer();
	printl("VFS: VFS is running.\n");

	MESSAGE msg;
	struct file_system * fs;
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
			printl("Doing open, %d\n", fs->open);
			msg.FD = fs->open(&msg);
			printl("Done open, %d\n", fs->open);
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

		/* reply */
		if (msg.type != SUSPEND_PROC) {
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
		}
	}
}


PUBLIC void register_filesystem(MESSAGE * m){
	struct file_system * tmp;
	memcpy(va2la(TASK_FS, tmp), va2la(m->source, m->BUF), sizeof(struct file_system));
	printl("VFS: Register filesystem: name: %s\n", tmp->name);
	if (file_systems == NULL) {
		file_systems = tmp;
	}
}

PRIVATE void dump_fs(struct file_system * fs){
	printl("%s { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d }\n",
		fs->name,
		fs->open,
		fs->close,
		fs->lseek,
		fs->chdir,
		fs->chroot,
		fs->mount,
		fs->umount,
		fs->mkdir,
		fs->rdwt,
		fs->unlink,
		fs->stat,
		fs->fork,
		fs->exit);
};
