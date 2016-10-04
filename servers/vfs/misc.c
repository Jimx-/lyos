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
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "errno.h"
#include <page.h>
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

PRIVATE int change_directory(struct fproc* fp, struct inode ** ppin, endpoint_t src, char * pathname, int len);
PRIVATE int change_node(struct fproc* fp, struct inode ** ppin, struct inode * pin);

PUBLIC int vfs_verify_endpt(endpoint_t ep, int * proc_nr)
{
    *proc_nr = ENDPOINT_P(ep);
    return 0;
}

PUBLIC struct fproc * vfs_endpt_proc(endpoint_t ep)
{
    int proc_nr;
    if (vfs_verify_endpt(ep, &proc_nr) == 0) return &fproc_table[proc_nr];
    return NULL;
}

PUBLIC void lock_fproc(struct fproc* fp)
{
    pthread_mutex_lock(&fp->lock);
}

PUBLIC void unlock_fproc(struct fproc* fp)
{
    pthread_mutex_unlock(&fp->lock);
}

PUBLIC int do_fcntl(MESSAGE * p)
{
    int fd = p->FD;
    int request = p->REQUEST;
    int argx = p->BUF_LEN;
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);
    int retval = 0;

    struct file_desc * filp = get_filp(pcaller, fd, RWL_READ);
    if (!filp) return EBADF;

    int i, newfd;
    switch(request) {
        case F_DUPFD:
            if (argx < 0 || argx >= NR_FILES) return -EINVAL;

            newfd = -1;
            retval = get_fd(pcaller, argx, &newfd, NULL);
            if (retval) {
                unlock_filp(filp);
                return retval;
            }
            filp->fd_cnt++;
            pcaller->filp[newfd] = filp;
            retval = newfd;
            break;
        case F_SETFD:
            break;
        case F_SETFL:
            filp->fd_mode = argx;
            break;
        default:
            break;
    }

    unlock_filp(filp);
    return retval;
}

/**
 * <Ring 1> Perform the DUP and DUP2 syscalls.
 */
PUBLIC int do_dup(MESSAGE * p)
{
    int fd = p->FD;
    int newfd = p->NEWFD;
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);
    int retval = 0;

    struct file_desc* filp = get_filp(pcaller, fd, RWL_READ);
    if (!filp) return EBADF;
    
    if (newfd == -1) {
        /* find a free slot in PROCESS::filp[] */
        retval = get_fd(pcaller, 0, &newfd, NULL);
        if (retval) {
            unlock_filp(filp);
            return retval;
        }
    }

    if (pcaller->filp[newfd] != 0) {
        /* close the file */
        p->FD = newfd;
        do_close(p);
    }

    filp->fd_cnt++;
    pcaller->filp[newfd] = filp;
    unlock_filp(filp);

    return newfd;
}

/**
 * <Ring 1> Perform the CHDIR syscall.
 */
PUBLIC int do_chdir(MESSAGE * p)
{
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);
    return change_directory(pcaller, &pcaller->pwd, p->source, p->PATHNAME, p->NAME_LEN);
}

/**
 * <Ring 1> Perform the FCHDIR syscall.
 */
PUBLIC int do_fchdir(MESSAGE * p)
{
    endpoint_t src = p->source;
    struct fproc* pcaller = vfs_endpt_proc(src);

    struct file_desc* filp = get_filp(pcaller, p->FD, RWL_READ);
    if (!filp) return EBADF;

    int retval = change_node(pcaller, &pcaller->pwd, filp->fd_inode);
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
PRIVATE int change_directory(struct fproc* fp, struct inode ** ppin, endpoint_t src, char * string, int len)
{
    char pathname[MAX_PATH];
    if (len > MAX_PATH) return ENAMETOOLONG;

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
PRIVATE int change_node(struct fproc* fp, struct inode ** ppin, struct inode * pin)
{
    int retval = 0;

    /* nothing to do */
    if (*ppin == pin) return 0;

    /* must be a directory */
    if ((pin->i_mode & I_TYPE) != I_DIRECTORY) {
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

PUBLIC int do_mm_request(MESSAGE* m)
{
    int req_type = m->MMRTYPE;
    int fd = m->MMRFD;
    int result = 0;
    endpoint_t ep = m->MMRENDPOINT;
    struct fproc* fp = vfs_endpt_proc(ep);
    struct fproc* mm_task = vfs_endpt_proc(TASK_MM);
    size_t len = m->MMRLENGTH;
    off_t offset = m->MMROFFSET;
    phys_bytes buf = m->MMRBUF;

    if (!mm_task) panic("mm not present!");

    if (m->source != TASK_MM) return EPERM;
    switch (req_type) {
    case MMR_FDLOOKUP:
        {
            if (!fp) {
                result = ESRCH;
                goto reply;
            }

            struct file_desc* filp = get_filp(fp, fd, RWL_WRITE);
            if (!filp || !filp->fd_inode) {
                result = EBADF;
                goto reply;
            }

            lock_fproc(mm_task);
            int mmfd;
            result = get_fd(mm_task, 0, &mmfd, NULL);
            if (result) {
                goto reply;
            }

            filp->fd_cnt++;
            filp->fd_inode->i_cnt++;
            mm_task->filp[mmfd] = filp;
            unlock_fproc(mm_task);

            m->MMRDEV = filp->fd_inode->i_dev;
            m->MMRINO = filp->fd_inode->i_num;
            m->MMRLENGTH = filp->fd_inode->i_size;
            m->MMRFD = mmfd;

            unlock_filp(filp);
            result = 0;
            break;
        }

    case MMR_FDREAD:
        {
            struct file_desc* filp = get_filp(mm_task, fd, RWL_WRITE);
            if (!filp || !filp->fd_inode) {
                result = EBADF;
                goto reply;
            }

            struct inode* pin = filp->fd_inode;
            int file_type = pin->i_mode & I_TYPE;

            if (file_type == I_CHAR_SPECIAL) {
                cdev_io(CDEV_READ, pin->i_specdev, KERNEL, buf, offset, len);
            } else if (file_type == I_REGULAR) {
                size_t count;
                result = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, offset, READ, TASK_MM,
                    buf, len, NULL, &count);
                m->MMRLENGTH = count;
            } else if (file_type == I_DIRECTORY) {
                unlock_filp(filp);
                return EISDIR;
            } else {
                unlock_filp(filp);
                return EBADF;
            }

            unlock_filp(filp);
            break;
        }
        case MMR_FDCLOSE:
        {
            struct file_desc* filp = get_filp(mm_task, fd, RWL_READ);
            if (!filp || !filp->fd_inode) {
                result = EBADF;
                goto reply;
            }

            struct inode* pin = filp->fd_inode;

            unlock_inode(pin);
            put_inode(pin);

            if (--filp->fd_cnt == 0) {
                filp->fd_inode = NULL;
                mm_task->filp[fd] = NULL;
            }

            unlock_filp(filp);
        }
    }

reply:
    m->MMRRESULT = result;
    m->MMRENDPOINT = ep;

    m->type = MM_VFS_REPLY;
    if (send_recv(SEND, TASK_MM, m) != 0) panic("do_mm_request(): cannot reply to mm");

    return SUSPEND;
}

/* Perform fs part of fork/exit */
PUBLIC int fs_fork(MESSAGE * p)
{
    int i;
    struct fproc * child = vfs_endpt_proc(p->ENDPOINT);
    struct fproc * parent = vfs_endpt_proc(p->PENDPOINT);
    pthread_mutex_t cmutex;

    if (child == NULL || parent == NULL) {
        return EINVAL;
    }
    
    lock_fproc(parent);
    lock_fproc(child);

    cmutex = child->lock;
    *child = *parent;
    child->lock = cmutex;
    child->pid = p->PID;
    child->endpoint = p->ENDPOINT;

    for (i = 0; i < NR_FILES; i++) {
        struct file_desc* filp = get_filp(child, i, RWL_WRITE);
        if (filp) {
            filp->fd_cnt++;
            filp->fd_inode->i_cnt++;
            unlock_filp(filp);
        }
    }

    if (child->root) child->root->i_cnt++;
    if (child->pwd) child->pwd->i_cnt++;

    unlock_fproc(child);
    unlock_fproc(parent);
    return 0;
}

PUBLIC int fs_exit(MESSAGE * m)
{
    int i;
    struct fproc * p = vfs_endpt_proc(m->ENDPOINT);

    for (i = 0; i < NR_FILES; i++) {
        struct file_desc* filp = get_filp(p, i, RWL_WRITE);
        if (filp) {
            p->filp[i]->fd_inode->i_cnt--;
            if (--p->filp[i]->fd_cnt == 0) {
                p->filp[i]->fd_inode = 0;
            }
            unlock_filp(filp);
            p->filp[i] = 0;
        }
    }

    return 0;
}
