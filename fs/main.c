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
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
  
#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

/**
 * Missing filesystem syscalls:
 * link/unlink
 * chmod/chown/chroot
 * fcntl
 * ioctl
 * mkdir/mknod/rmdir
 * pipe
 * rename
 * stime
 * umount
 * utime
 */


PUBLIC void init_vfs();

PRIVATE int fs_fork(MESSAGE * p);
PRIVATE int fs_exit(MESSAGE * m);

/**
 * <Ring 1> Main loop of VFS.
 */
PUBLIC void task_fs()
{
	printl("VFS: Virtual filesystem is running.\n");

	init_vfs();
	
	MESSAGE msg;
	while (1) {
		send_recv(RECEIVE, ANY, &msg);
		int src = msg.source;
		pcaller = &proc_table[src];
		int msgtype = msg.type;

		switch (msgtype) {
        case FS_REGISTER:
            msg.RETVAL = do_register_filesystem(&msg);
            break;
		case OPEN:
			msg.FD = do_open(&msg);
			break;
		case CLOSE:
			msg.RETVAL = do_close(&msg);
			break;
		case READ:
		case WRITE:
			msg.CNT = do_rdwt(&msg);
			break;
		case IOCTL:
			msg.RETVAL = do_ioctl(&msg);
			break;
		case STAT:
			msg.RETVAL = do_stat(&msg);
			break;
		case FSTAT:
			msg.RETVAL = do_fstat(&msg);
			break;
		case ACCESS:
			msg.RETVAL = do_access(&msg);
			break;
		case LSEEK:
			msg.RETVAL = do_lseek(&msg);
			break;
		case UMASK:
			msg.RETVAL = (int)do_umask(&msg);
			break;
		case DUP:
			msg.RETVAL = do_dup(&msg);
			break;
		case CHDIR:
			msg.RETVAL = do_chdir(&msg);
			break;
		case FCHDIR:
			msg.RETVAL = do_fchdir(&msg);
			break;
		case MOUNT:
			msg.RETVAL = do_mount(&msg);
			break;
		case FORK:
			msg.RETVAL = fs_fork(&msg);
			break;
		case EXEC:
			msg.RETVAL = do_exec(&msg);
			break;
		case EXIT:
			msg.RETVAL = fs_exit(&msg);
			break;
		case RESUME_PROC:
			src = msg.PROC_NR;
			break;
		default:
			dump_msg("VFS::unknown msg", &msg);
			assert(0);
			break;
		}

		if (msg.type != SUSPEND_PROC) {
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
		}
	}
}

PUBLIC void init_vfs()
{
	int i;

	/* f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		memset(&f_desc_table[i], 0, sizeof(struct file_desc));

	/* inode_table[] */
	for (i = 0; i < NR_INODE; i++)
		memset(&inode_table[i], 0, sizeof(struct inode));

    init_inode_table();

    int initrd_dev = MAKE_DEV(DEV_RD, MINOR_INITRD);
    // mount root
    mount_fs(initrd_dev, "/", TASK_INITFS, 0);
    printl("VFS: Mounted init ramdisk\n");

    add_filesystem(TASK_INITFS, "initfs");
    add_filesystem(TASK_DEVMAN, "sysfs");
}

/* Perform fs part of fork/exit */
PRIVATE int fs_fork(MESSAGE * p)
{
	int i;
	struct proc * child = &proc_table[p->PID];
	for (i = 0; i < NR_FILES; i++) {
		if (child->filp[i]) {
			child->filp[i]->fd_cnt++;
			child->filp[i]->fd_inode->i_cnt++;
		}
	}

	if (child->root) child->root->i_cnt++;
	if (child->pwd) child->pwd->i_cnt++;

	return 0;
}

PRIVATE int fs_exit(MESSAGE * m)
{
	int i;
	struct proc * p = &proc_table[m->PID];
	for (i = 0; i < NR_FILES; i++) {
		if (p->filp[i]) {
			p->filp[i]->fd_inode->i_cnt--;
			if (--p->filp[i]->fd_cnt == 0) {
				p->filp[i]->fd_inode = 0;
			}
			p->filp[i] = 0;
		}
	}
	return 0;
}
