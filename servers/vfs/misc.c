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
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <asm/page.h>
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"
#include "thread.h"

static int change_directory(struct fproc* fp, struct inode** ppin,
                            endpoint_t src, char* pathname, int len);
static int change_node(struct fproc* fp, struct inode** ppin,
                       struct inode* pin);

int vfs_verify_endpt(endpoint_t ep, int* proc_nr)
{
    *proc_nr = ENDPOINT_P(ep);
    return 0;
}

struct fproc* vfs_endpt_proc(endpoint_t ep)
{
    int proc_nr;
    if (vfs_verify_endpt(ep, &proc_nr) == 0) return &fproc_table[proc_nr];
    return NULL;
}

void lock_fproc(struct fproc* fp)
{
    int retval;
    struct worker_thread* old_self;

    retval = mutex_trylock(&fp->lock);
    if (retval == 0) {
        return;
    }

    /* need blocking */
    old_self = worker_suspend(WT_BLOCKED_ON_LOCK);

    retval = mutex_lock(&fp->lock);
    if (retval != 0) {
        panic("failed to acquire proc lock: %d", retval);
    }

    worker_resume(old_self);
}

void unlock_fproc(struct fproc* fp) { mutex_unlock(&fp->lock); }

int do_fcntl()
{
    int fd = self->msg_in.FD;
    int request = self->msg_in.REQUEST;
    void* arg = self->msg_in.BUF;
    int argx = self->msg_in.BUF_LEN;
    int flags;
    int retval = 0;

    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return EBADF;

    int newfd;
    switch (request) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
        if (argx < 0 || argx >= NR_FILES) return -EINVAL;

        newfd = -1;
        retval = get_fd(fproc, argx, 0, &newfd, NULL);
        if (retval) {
            unlock_filp(filp);
            return retval;
        }
        filp->fd_cnt++;
        fproc->filp[newfd] = filp;
        retval = newfd;
        break;
    case F_GETFD:
        retval = 0;
        break;
    case F_SETFD:
        break;

    case F_GETFL:
        flags = filp->fd_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
        retval = flags;
        break;

    case F_SETFL:
        flags = O_NONBLOCK | O_APPEND;
        filp->fd_flags = (filp->fd_flags & ~flags) | (argx & flags);
        break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
        retval = do_lock(fd, request, arg);
        break;
    default:
        retval = -EINVAL;
        break;
    }

    unlock_filp(filp);
    return retval;
}

/**
 * <Ring 1> Perform the DUP and DUP2 syscalls.
 */
int do_dup(void)
{
    int fd = self->msg_in.FD;
    int newfd = self->msg_in.NEWFD;
    int retval = 0;

    struct file_desc* filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return -EBADF;

    if (newfd != -1 && fd == newfd) {
        unlock_filp(filp);
        return newfd;
    }

    if (newfd == -1) {
        /* find a free slot in PROCESS::filp[] */
        retval = get_fd(fproc, 0, 0, &newfd, NULL);
        if (retval) {
            unlock_filp(filp);
            return -retval;
        }
    }

    if (fproc->filp[newfd] != NULL) {
        /* close the file */
        self->msg_out.FD = newfd;
        close_fd(fproc, newfd);
    }

    filp->fd_cnt++;
    fproc->filp[newfd] = filp;
    unlock_filp(filp);

    return newfd;
}

/**
 * <Ring 1> Perform the CHDIR syscall.
 */
int do_chdir(void)
{
    return change_directory(fproc, &fproc->pwd, fproc->endpoint,
                            self->msg_in.PATHNAME, self->msg_in.NAME_LEN);
}

/**
 * <Ring 1> Perform the FCHDIR syscall.
 */
int do_fchdir(void)
{
    struct file_desc* filp = get_filp(fproc, self->msg_in.FD, RWL_READ);
    if (!filp) return EBADF;

    int retval = change_node(fproc, &fproc->pwd, filp->fd_inode);
    unlock_filp(filp);
    return retval;
}

/**
 * <Ring 1> Change the directory.
 * @param  ppin     Directory to be change.
 * @param  string   Pathname.
 * @param  len      Length of pathname.
 * @return          Zero on success.
 */
static int change_directory(struct fproc* fp, struct inode** ppin,
                            endpoint_t src, char* string, int len)
{
    char pathname[PATH_MAX];
    if (len > PATH_MAX) return ENAMETOOLONG;

    /* fetch the name */
    data_copy(SELF, pathname, src, string, len);
    pathname[len] = '\0';

    struct lookup lookup;
    struct vfs_mount* vmnt = NULL;
    struct inode* pin = NULL;
    init_lookup(&lookup, pathname, 0, &vmnt, &pin);
    lookup.vmnt_lock = RWL_READ;
    lookup.inode_lock = RWL_WRITE;
    pin = resolve_path(&lookup, fp);
    if (!pin) return err_code;

    int retval = change_node(fp, ppin, pin);

    unlock_inode(pin);
    unlock_vmnt(vmnt);
    put_inode(pin);

    return retval;
}

/**
 * <Ring 1> Change ppin into pin.
 */
static int change_node(struct fproc* fp, struct inode** ppin, struct inode* pin)
{
    int retval = 0;

    /* nothing to do */
    if (*ppin == pin) return 0;

    /* must be a directory */
    if (!S_ISDIR(pin->i_mode)) {
        retval = ENOTDIR;
    } else {
        /* must be searchable */
        retval = forbidden(fp, pin, X_BIT);
    }

    if (retval == 0) {
        put_inode(*ppin);
        pin->i_cnt++;
        *ppin = pin;
    }

    return retval;
}

int do_mm_request(void)
{
    int req_type = self->msg_in.MMRTYPE;
    int fd = self->msg_in.MMRFD;
    ssize_t result = 0;
    endpoint_t ep = self->msg_in.MMRENDPOINT;
    struct fproc* fp = vfs_endpt_proc(ep);
    struct fproc* mm_task = vfs_endpt_proc(TASK_MM);
    size_t len = self->msg_in.MMRLENGTH;
    loff_t offset = self->msg_in.MMROFFSET;
    phys_bytes buf = (phys_bytes)self->msg_in.MMRBUF;
    void* vaddr = self->msg_in.MMRBUF;

    if (!mm_task) panic("mm not present!");

    if (self->msg_in.source != TASK_MM) return EPERM;
    switch (req_type) {
    case MMR_FDLOOKUP: {
        if (!fp) {
            result = ESRCH;
            goto reply;
        }

        struct file_desc* filp = get_filp(fp, fd, RWL_WRITE);

        if (!filp || !filp->fd_inode) {
            result = EBADF;
            goto reply;
        }

        int mmfd;
        result = get_fd(mm_task, 0, 0, &mmfd, NULL);
        if (result) {
            goto reply;
        }

        filp->fd_cnt++;
        mm_task->filp[mmfd] = filp;

        self->msg_out.MMRDEV = filp->fd_inode->i_dev;
        self->msg_out.MMRINO = filp->fd_inode->i_num;
        self->msg_out.MMRMODE = filp->fd_inode->i_mode;
        self->msg_out.MMRFD = mmfd;

        unlock_filp(filp);

        result = 0;
        break;
    }

    case MMR_FDREAD: {
        struct file_desc* filp = get_filp(mm_task, fd, RWL_WRITE);
        if (!filp || !filp->fd_inode) {
            result = EBADF;
            goto reply;
        }

        struct inode* pin = filp->fd_inode;

        if (S_ISDIR(pin->i_mode)) {
            unlock_filp(filp);
            result = EISDIR;
            goto reply;
        } else if (!filp->fd_fops || !filp->fd_fops->read) {
            unlock_filp(filp);
            result = EBADF;
            goto reply;
        } else {
            result =
                filp->fd_fops->read(filp, (void*)buf, len, &offset, mm_task);

            if (result < 0) {
                result = -result;
            } else {
                self->msg_out.MMRLENGTH = result;
                result = 0;
            }
        }

        unlock_filp(filp);
        break;
    }
    case MMR_FDMMAP: {
        struct file_desc* filp = get_filp(mm_task, fd, RWL_WRITE);
        if (!filp || !filp->fd_inode) {
            result = EBADF;
            goto reply;
        }

        struct inode* pin = filp->fd_inode;
        void* retaddr;

        if (S_ISCHR(pin->i_mode)) {
            result =
                cdev_mmap(pin->i_specdev, ep, vaddr, offset, len, &retaddr, fp);
            if (result) {
                unlock_filp(filp);
                goto reply;
            }

            self->msg_out.MMRBUF = retaddr;
        } else { /* error if MM is trying to map a non-device file */
            unlock_filp(filp);
            result = EBADF;
            goto reply;
        }

        unlock_filp(filp);
        break;
    }
    case MMR_FDCLOSE: {
        result = close_fd(mm_task, fd);
        break;
    }
    }

reply:
    if (result != SUSPEND) {
        self->msg_out.MMRRESULT = result;
        self->msg_out.MMRENDPOINT = ep;

        self->msg_out.type = MM_VFS_REPLY;
        if (asyncsend3(TASK_MM, &self->msg_out, 0) != 0)
            panic("vfs: do_mm_request(): cannot reply to mm");
    }

    return SUSPEND;
}

/* Perform fs part of fork/exit */
int fs_fork(void)
{
    int i;
    struct fproc* child = vfs_endpt_proc(self->msg_in.ENDPOINT);
    struct fproc* parent = vfs_endpt_proc(self->msg_in.PENDPOINT);
    mutex_t cmutex;
    struct wait_queue_head signalfd_wq;

    if (child == NULL || parent == NULL) {
        return EINVAL;
    }

    lock_fproc(parent);
    lock_fproc(child);

    cmutex = child->lock;
    *child = *parent;
    child->lock = cmutex;

    init_waitqueue_head(&child->signalfd_wq);

    child->pid = self->msg_in.PID;
    child->endpoint = self->msg_in.ENDPOINT;
    child->flags |= FPF_INUSE;

    for (i = 0; i < NR_FILES; i++) {
        struct file_desc* filp = child->filp[i];
        if (filp) {
            filp->fd_cnt++;
        }
    }

    if (child->root) child->root->i_cnt++;
    if (child->pwd) child->pwd->i_cnt++;

    unlock_fproc(child);
    unlock_fproc(parent);
    return 0;
}

int fs_exit()
{
    int i;
    struct fproc* p = vfs_endpt_proc(self->msg_in.ENDPOINT);

    p->flags &= ~FPF_INUSE;

    for (i = 0; i < NR_FILES; i++) {
        close_fd(p, i);
    }

    if (p->pwd) {
        put_inode(p->pwd);
        p->pwd = NULL;
    }
    if (p->root) {
        put_inode(p->root);
        p->root = NULL;
    }

    return 0;
}

int request_sync(endpoint_t fs_ep)
{
    MESSAGE m;

    m.type = FS_SYNC;

    return fs_sendrec(fs_ep, &m);
}

int do_sync(void)
{
    int retval = 0;
    struct vfs_mount* vmnt;

    list_for_each_entry(vmnt, &vfs_mount_table, list)
    {
        retval = lock_vmnt(vmnt, RWL_READ);
        if (retval) break;

        if (vmnt->m_dev != NO_DEV && vmnt->m_fs_ep != NO_TASK &&
            vmnt->m_root_node != NULL) {
            request_sync(vmnt->m_fs_ep);
        }

        unlock_vmnt(vmnt);
    }

    return retval;
}
