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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include <stdlib.h>
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include <elf.h>
#include "lyos/param.h"
#include <lyos/sysutils.h>
#include <libcoro/libcoro.h>
#include "global.h"
#include "thread.h"

//#define DEBUG
#ifdef DEBUG
#define DEB(x)       \
    printl("VFS: "); \
    x
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
static void dispatch_work(MESSAGE* msg, void (*func)(void));
static void do_work(void);
static void do_reply(struct worker_thread* worker, MESSAGE* msg);
static void init_root(void);

/**
 * <Ring 1> Main loop of VFS.
 */
PUBLIC int main()
{
    MESSAGE msg;
    int txn_id;
    struct worker_thread* wp;

    printl("VFS: Virtual filesystem is running.\n");

    coro_init();
    init_vfs();

    while (TRUE) {
        worker_yield();

        send_recv(RECEIVE_ASYNC, ANY, &msg);
        fproc = vfs_endpt_proc(msg.source);

        txn_id = VFS_TXN_GET_ID(msg.type);
        if (IS_VFS_TXN_ID(txn_id)) {
            wp = worker_get((thread_t)(txn_id - FS_TXN_ID));
            if (wp == NULL || wp->fproc == NULL) {
                printl("VFS: spurious message to worker %d from %d\n", wp->tid,
                       msg.source);
                continue;
            }

            msg.type = VFS_TXN_GET_TYPE(msg.type);
            do_reply(wp, &msg);
            continue;
        }

        switch (msg.type) {
        case CDEV_REPLY:
        case CDEV_MMAP_REPLY:
        case CDEV_SELECT_REPLY1:
        case CDEV_SELECT_REPLY2:
            cdev_reply(&msg);
            break;
        case PM_VFS_EXEC:
            fproc = vfs_endpt_proc(msg.ENDPOINT);
            dispatch_work(&msg, do_work);
            break;
        default:
            dispatch_work(&msg, do_work);
            break;
        }
    }

    return 0;
}

static void dispatch_work(MESSAGE* msg, void (*func)(void))
{
    worker_dispatch(fproc, func, msg);
}

static void do_work(void)
{
    int msgtype = self->msg_in.type;

    memset(&self->msg_out, 0, sizeof(MESSAGE));

    switch (msgtype) {
    case FS_REGISTER:
        self->msg_out.RETVAL = do_register_filesystem();
        break;
    case OPEN:
        self->msg_out.FD = do_open();
        break;
    case CLOSE:
        self->msg_out.RETVAL = do_close();
        break;
    case READ:
    case WRITE:
        self->msg_out.RETVAL = do_rdwt();
        break;
    case IOCTL:
        self->msg_out.RETVAL = do_ioctl();
        break;
    case STAT:
        self->msg_out.RETVAL = do_stat();
        break;
    case FSTAT:
        self->msg_out.RETVAL = do_fstat();
        break;
    case ACCESS:
        self->msg_out.RETVAL = do_access();
        break;
    case LSEEK:
        self->msg_out.RETVAL = do_lseek();
        break;
    case MKDIR:
        self->msg_out.RETVAL = do_mkdir();
        break;
    case UMASK:
        self->msg_out.RETVAL = (int)do_umask();
        break;
    case FCNTL:
        self->msg_out.RETVAL = do_fcntl();
        break;
    case DUP:
        self->msg_out.RETVAL = do_dup();
        break;
    case CHDIR:
        self->msg_out.RETVAL = do_chdir();
        break;
    case FCHDIR:
        self->msg_out.RETVAL = do_fchdir();
        break;
    case MOUNT:
        self->msg_out.RETVAL = do_mount();
        break;
    case CHMOD:
    case FCHMOD:
        self->msg_in.RETVAL = do_chmod(msgtype);
        break;
    case GETDENTS:
        self->msg_out.RETVAL = do_getdents();
        break;
    case PM_VFS_GETSETID:
        self->msg_out.RETVAL = fs_getsetid();
        break;
    case PM_VFS_FORK:
        self->msg_out.RETVAL = fs_fork();
        break;
    case EXIT:
        self->msg_out.RETVAL = fs_exit();
        break;
    case PM_VFS_EXEC:
        self->msg_out.RETVAL = fs_exec();
        break;
    case MM_VFS_REQUEST:
        self->msg_out.RETVAL = do_mm_request();
        break;
    default:
        self->msg_out.RETVAL = ENOSYS;
        break;
    }

    if (self->msg_out.type != SUSPEND_PROC && self->msg_out.RETVAL != SUSPEND) {
        self->msg_out.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, self->msg_in.source, &self->msg_out);
    }
}

static void do_reply(struct worker_thread* worker, MESSAGE* msg)
{
    if (worker->recv_from != msg->source) {
        printl("VFS: thread %d waiting for %d to reply, not %d\n", worker->tid,
               worker->recv_from, msg->source);
        return;
    }

    if (worker->msg_recv == NULL) {
        return;
    }

    *worker->msg_recv = *msg;
    worker->msg_recv = NULL;
    worker->recv_from = NO_TASK;

    worker_wake(worker);
}

PUBLIC void init_vfs()
{
    int i;

    /* f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++) {
        memset(&f_desc_table[i], 0, sizeof(struct file_desc));
        mutex_init(&f_desc_table[i].fd_lock, NULL);
    }

    /* inode_table[] */
    for (i = 0; i < NR_INODE; i++)
        memset(&inode_table[i], 0, sizeof(struct inode));

    /* fproc_table[] */
    for (i = 0; i < NR_PROCS; i++) {
        mutex_init(&fproc_table[i].lock, NULL);
        fproc_table[i].worker = NULL;
        fproc_table[i].flags = 0;
    }

    init_inode_table();
    init_select();

    /* initialize system procs */
    MESSAGE pm_msg;
    struct fproc* pfp;
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
    } while (TRUE);

    pm_msg.RETVAL = 0;
    send_recv(SEND, TASK_PM, &pm_msg);

    worker_init();
    printl("VFS: Started %d worker thread(s)\n", NR_WORKER_THREADS);

    MESSAGE msg;
    worker_dispatch(vfs_endpt_proc(TASK_FS), init_root, &msg);

    add_filesystem(TASK_SYSFS, "sysfs");
    add_filesystem(TASK_DEVMAN, "devfs");

    get_sysinfo(&sysinfo);
    system_hz = get_system_hz();
}

static void init_root(void)
{
    worker_allow(FALSE);

    int initrd_dev = MAKE_DEV(DEV_RD, MINOR_INITRD);
    // mount root
    if (mount_fs(initrd_dev, "/", TASK_INITFS, 0) != 0) {
        panic("failed to mount initial ramdisk");
    }

    printl("VFS: Mounted init ramdisk\n");

    worker_allow(TRUE);
}
