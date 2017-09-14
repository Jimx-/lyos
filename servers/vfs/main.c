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
#include <lyos/ipc.h>
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "lyos/param.h"
#include <lyos/sysutils.h>
#include "global.h"
#include "thread.h"

//#define DEBUG
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

/**
 * <Ring 1> Main loop of VFS.
 */
PUBLIC int main()
{
	printl("VFS: Virtual filesystem is running.\n");

	init_vfs();
	
	MESSAGE msg;
	while (TRUE) {
		fs_sleeping = 1;
		send_recv(RECEIVE_ASYNC, ANY, &msg);
		fs_sleeping = 0;

		int msgtype = msg.type;
		/* enqueue a request from the user */
		if (msgtype != FS_THREAD_WAKEUP) {
            enqueue_request(&msg);
        }

		struct vfs_message* res;
		/* send result back to the user */
		lock_response_queue();
		while (TRUE) {
			res = dequeue_response();
			if (!res) break;

			if (res->msg.type != SUSPEND_PROC && res->msg.RETVAL != SUSPEND) {
				res->msg.type = SYSCALL_RET;
				send_recv(SEND_NONBLOCK, res->msg.source, &res->msg);
			}

			free(res);
		}
		unlock_response_queue();
	}

	return 0;
}

PUBLIC void init_vfs()
{
	int i;

	/* f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++) {
		memset(&f_desc_table[i], 0, sizeof(struct file_desc));
		pthread_mutex_init(&f_desc_table[i].fd_lock, NULL);
	}
	
	/* inode_table[] */
	for (i = 0; i < NR_INODE; i++)
		memset(&inode_table[i], 0, sizeof(struct inode));

	/* fproc_table[] */
	for (i = 0; i < NR_PROCS; i++) {
		pthread_mutex_init(&fproc_table[i].lock, NULL);
	}
	pthread_mutex_init(&filesystem_lock, NULL);

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
		pfp->flags |= FPF_INUSE;

		for (i = 0; i < NR_FILES; i++)
			pfp->filp[i] = 0;
    } while(TRUE);
    
    pm_msg.RETVAL = 0;
    send_recv(SEND, TASK_PM, &pm_msg);

    int id;
    for (id = 0; id < NR_WORKER_THREADS; id++) {
    	pid_t pid = create_worker(id);
    	if (pid > 0) nr_workers++;
    }
    printl("VFS: Started %d worker thread(s)\n", nr_workers);

    int initrd_dev = MAKE_DEV(DEV_RD, MINOR_INITRD);
    // mount root
    mount_fs(vfs_endpt_proc(TASK_FS), initrd_dev, "/", TASK_INITFS, 0);
    printl("VFS: Mounted init ramdisk\n");

    add_filesystem(TASK_SYSFS, "sysfs");

    fs_sleeping = 0;

    get_sysinfo(&sysinfo);
}
