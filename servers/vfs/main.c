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

#include <lyos/types.h>
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
#include <lyos/timer.h>
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
 * link
 * chroot
 * fcntl
 * rename
 * stime
 * umount
 */

void init_vfs();
static void dispatch_work(MESSAGE* msg, void (*func)(void));
static void do_work(void);
static void do_reply(struct worker_thread* worker, MESSAGE* msg);
static void init_root(void);

/**
 * <Ring 1> Main loop of VFS.
 */
int main()
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
        } else if (msg.type == NOTIFY_MSG) {
            switch (msg.source) {
            case CLOCK:
                expire_timer(msg.TIMESTAMP); /* select() needs this */
                break;
            }

            continue;
        }

        switch (msg.type) {
        case CDEV_REPLY:
        case CDEV_POLL_NOTIFY:
            cdev_reply(&msg);
            break;
        case SDEV_REPLY:
        case SDEV_SOCKET_REPLY:
        case SDEV_ACCEPT_REPLY:
        case SDEV_RECV_REPLY:
        case SDEV_POLL_NOTIFY:
            sdev_reply(&msg);
            break;
        case PM_VFS_EXEC:
            fproc = vfs_endpt_proc(msg.ENDPOINT);
            dispatch_work(&msg, do_work);
            break;
        case PM_SIGNALFD_REPLY:
        case PM_SIGNALFD_POLL_NOTIFY:
            do_pm_signalfd_reply(&msg);
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
    case OPENAT:
        self->msg_out.FD = do_openat();
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
    case LSTAT:
        self->msg_out.RETVAL = do_lstat();
        break;
    case FSTATAT:
        self->msg_out.RETVAL = do_fstatat();
        break;
    case SYMLINK:
        self->msg_out.RETVAL = do_symlink();
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
    case MKNOD:
        self->msg_out.RETVAL = do_mknod();
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
        self->msg_out.RETVAL = do_chmod(msgtype);
        break;
    case FCHOWNAT:
        self->msg_out.RETVAL = do_fchownat(msgtype);
        break;
    case GETDENTS:
        self->msg_out.RETVAL = do_getdents();
        break;
    case READLINK:
        self->msg_out.RETVAL = do_rdlink();
        break;
    case READLINKAT:
        self->msg_out.RETVAL = do_rdlinkat();
        break;
    case UNLINK:
    case RMDIR:
        self->msg_out.RETVAL = do_unlink();
        break;
    case UNLINKAT:
        self->msg_out.RETVAL = do_unlinkat();
        break;
    case SELECT:
        self->msg_out.RETVAL = do_select();
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
    case SYNC:
        self->msg_out.RETVAL = do_sync();
        break;
    case PIPE2:
        self->msg_out.u.m_vfs_fdpair.retval = do_pipe2();
        break;
    case POLL:
        self->msg_out.RETVAL = do_poll();
        break;
    case UTIMENSAT:
        self->msg_out.RETVAL = do_utimensat();
        break;
    case EVENTFD:
        self->msg_out.FD = do_eventfd();
        break;
    case SIGNALFD:
        self->msg_out.FD = do_signalfd();
        break;
    case TIMERFD_CREATE:
        self->msg_out.FD = do_timerfd_create();
        break;
    case TIMERFD_SETTIME:
        self->msg_out.RETVAL = do_timerfd_settime();
        break;
    case TIMERFD_GETTIME:
        self->msg_out.RETVAL = do_timerfd_gettime();
        break;
    case EPOLL_CREATE1:
        self->msg_out.FD = do_epoll_create1();
        break;
    case EPOLL_CTL:
        self->msg_out.RETVAL = do_epoll_ctl();
        break;
    case EPOLL_WAIT:
        self->msg_out.RETVAL = do_epoll_wait();
        break;
    case VFS_MAPDRIVER:
        self->msg_out.RETVAL = do_mapdriver();
        break;
    case SOCKET:
        self->msg_out.FD = do_socket();
        break;
    case VFS_SOCKETPATH:
        self->msg_out.u.m_vfs_socketpath.status = do_socketpath();
        break;
    case SOCKETPAIR:
        self->msg_out.u.m_vfs_fdpair.retval = do_socketpair();
        break;
    case BIND:
        self->msg_out.RETVAL = do_bind();
        break;
    case CONNECT:
        self->msg_out.RETVAL = do_connect();
        break;
    case LISTEN:
        self->msg_out.RETVAL = do_listen();
        break;
    case ACCEPT4:
        self->msg_out.FD = do_accept4();
        break;
    case SENDTO:
        self->msg_out.u.m_vfs_sendrecv.status = do_sendto();
        break;
    case RECVFROM:
        self->msg_out.u.m_vfs_sendrecv.status = do_recvfrom();
        break;
    case SENDMSG:
    case RECVMSG:
        self->msg_out.u.m_vfs_sendrecv.status = do_sockmsg();
        break;
    case GETSOCKOPT:
        self->msg_out.u.m_vfs_sockopt.status = do_getsockopt();
        break;
    case SETSOCKOPT:
        self->msg_out.u.m_vfs_sockopt.status = do_setsockopt();
        break;
    case GETSOCKNAME:
        self->msg_out.RETVAL = do_getsockname();
        break;
    case VFS_COPYFD:
        self->msg_out.RETVAL = do_copyfd();
        break;
    case INOTIFY_INIT1:
        self->msg_out.FD = do_inotify_init1();
        break;
    case INOTIFY_ADD_WATCH:
        self->msg_out.RETVAL = do_inotify_add_watch();
        break;
    default:
        self->msg_out.RETVAL = ENOSYS;
        break;
    }

    if (self->msg_out.RETVAL != SUSPEND) {
        self->msg_out.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, self->msg_in.source, &self->msg_out);
    }
}

static void do_reply(struct worker_thread* worker, MESSAGE* msg)
{
    if (worker->blocked_on != WT_BLOCKED_ON_DRV_MSG) return;

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

void init_vfs()
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
        init_waitqueue_head(&fproc_table[i].signalfd_wq);
    }

    init_cdev();
    init_sdev();
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
        pfp->ngroups = 0;
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
    add_filesystem(TASK_DEVMAN, "devtmpfs");

    get_sysinfo(&sysinfo);
    system_hz = get_system_hz();
}

static void init_root(void)
{
    worker_allow(FALSE);

    mount_pipefs();
    mount_anonfs();
    mount_sockfs();

    int initrd_dev = MAKE_DEV(DEV_RD, MINOR_INITRD);
    // mount root
    if (mount_fs(initrd_dev, "/", TASK_INITFS, 0, NO_TASK, NULL) != 0) {
        panic("failed to mount initial ramdisk");
    }

    printl("VFS: Mounted init ramdisk\n");

    worker_allow(TRUE);
}
