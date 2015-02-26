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
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "lyos/param.h"
#include "global.h"

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("VFS: "); x
#else
#define DEB(x)
#endif

/**
 * Missing filesystem syscalls:
 * link/unlink
 * chown/chroot
 * fcntl
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
PUBLIC int main()
{
	printl("VFS: Virtual filesystem is running.\n");

	init_vfs();
	
	MESSAGE msg;
	while (1) {
		send_recv(RECEIVE, ANY, &msg);
		int src = msg.source;
		pcaller = vfs_endpt_proc(src);
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
		case FCNTL:
			msg.RETVAL = do_fcntl(&msg);
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
		case CHMOD:
		case FCHMOD:
			msg.RETVAL = do_chmod(msgtype, &msg);
			break;
		case GETDENTS:
			msg.RETVAL = do_getdents(&msg);
			break;
		case PM_VFS_FORK:
			msg.RETVAL = fs_fork(&msg);
			break;
		case PM_VFS_GETSETID:
			msg.RETVAL = fs_getsetid(&msg);
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
			msg.RETVAL = ENOSYS;
			break;
		}

		if (msg.type != SUSPEND_PROC && msg.RETVAL != SUSPEND) {
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
		}
	}

	return 0;
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

    /* initialize system procs */
    MESSAGE pm_msg;
    struct fproc * pfp;
    do {
    	send_recv(RECEIVE, TASK_PM, &pm_msg);

    	if (pm_msg.type != PM_VFS_INIT) panic("bad msg type from pm");

    	if (pm_msg.ENDPOINT == NO_TASK) break;

    	pfp = &fproc_table[pm_msg.PROC_NR];
    	pfp->endpoint = pm_msg.ENDPOINT;
    	pfp->pid = pm_msg.PID;
    	pfp->realuid = pfp->effuid = SU_UID;
    	pfp->realgid = pfp->effgid = (gid_t)0;
    	pfp->pwd = NULL;
		pfp->root = NULL;
		pfp->umask = ~0;

		for (i = 0; i < NR_FILES; i++)
			pfp->filp[i] = 0;
    } while(TRUE);
    
    pm_msg.RETVAL = 0;
    send_recv(SEND, TASK_PM, &pm_msg);

    int initrd_dev = MAKE_DEV(DEV_RD, MINOR_INITRD);
    // mount root
    mount_fs(initrd_dev, "/", TASK_INITFS, 0);
    printl("VFS: Mounted init ramdisk\n");
}

/* Perform fs part of fork/exit */
PRIVATE int fs_fork(MESSAGE * p)
{
	int i;
	struct fproc * child = vfs_endpt_proc(p->ENDPOINT);
	struct fproc * parent = vfs_endpt_proc(p->PENDPOINT);
	if (child == NULL || parent == NULL) return EINVAL;
	
	*child = *parent;
	child->pid = p->PID;

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
	struct fproc * p = vfs_endpt_proc(m->ENDPOINT);
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
