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
#include <lyos/sysutils.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "errno.h"
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

static ssize_t vfs_read(struct file_desc* filp, char* buf, size_t count,
                        loff_t* ppos, struct fproc* fp)
{
    size_t bytes = 0;
    int retval;
    struct inode* pin = filp->fd_inode;

    retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, *ppos,
                               READ /* rw_flag */, fp->endpoint, buf, count,
                               ppos, &bytes);

    if (retval) {
        return -retval;
    }

    return bytes;
}

static ssize_t vfs_write(struct file_desc* filp, const char* buf, size_t count,
                         loff_t* ppos, struct fproc* fp)
{
    size_t bytes = 0;
    int retval;
    struct inode* pin = filp->fd_inode;
    loff_t position = *ppos;

    /* check for O_APPEND */
    if (filp->fd_mode & O_APPEND) position = pin->i_size;

    retval = request_readwrite(pin->i_fs_ep, pin->i_dev, pin->i_num, position,
                               WRITE /* rw_flag */, fp->endpoint, buf, count,
                               ppos, &bytes);

    if (retval) {
        return -retval;
    }

    return bytes;
}

const struct file_operations vfs_fops = {
    .read = vfs_read,
    .write = vfs_write,
};

void lock_filp(struct file_desc* filp, rwlock_type_t lock_type)
{
    int retval;
    struct worker_thread* old_self;

    if (filp->fd_inode) lock_inode(filp->fd_inode, lock_type);

    retval = mutex_trylock(&filp->fd_lock);
    if (retval == 0) {
        return;
    }

    /* need blocking */
    old_self = worker_suspend();

    retval = mutex_lock(&filp->fd_lock);
    if (retval != 0) {
        panic("failed to acquire filp lock: %d", retval);
    }

    worker_resume(old_self);
}

void unlock_filp(struct file_desc* filp)
{
    int retval;

    if (filp->fd_cnt > 0) {
        unlock_inode(filp->fd_inode);
    }

    retval = mutex_unlock(&filp->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }
}

void unlock_filps(struct file_desc* filp1, struct file_desc* filp2)
{
    int retval;

    if (filp1->fd_cnt > 0 && filp2->fd_cnt > 0) {
        unlock_inode(filp1->fd_inode);
    }

    retval = mutex_unlock(&filp1->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }

    retval = mutex_unlock(&filp2->fd_lock);
    if (retval != 0) {
        panic("failed to unlock filp: %d", retval);
    }
}

struct file_desc* alloc_filp()
{
    int i;

    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++) {
        struct file_desc* filp = &f_desc_table[i];

        if (f_desc_table[i].fd_inode == 0 && !mutex_trylock(&filp->fd_lock)) {
            return filp;
        }
    }

    return NULL;
}

int check_fds(struct fproc* fp, int nfds)
{
    int i;

    for (i = 0; i < NR_FILES; i++) {
        if (fp->filp[i] == NULL) {
            if (--nfds == 0) {
                return 0;
            }
        }
    }

    return EMFILE;
}

int get_fd(struct fproc* fp, int start, int* fd, struct file_desc** fpp)
{
    /* find an unused fd in proc's filp table and a free file slot */
    int i;

    for (i = start; i < NR_FILES; i++) {
        if (fp->filp[i] == 0) {
            *fd = i;
            break;
        }
    }
    if (i == NR_FILES) {
        return EMFILE;
    }
    if (!fpp) return 0;

    /* find a free slot in f_desc_table[] */
    for (i = 0; i < NR_FILE_DESC; i++) {
        struct file_desc* filp = &f_desc_table[i];

        if (filp->fd_inode == 0 && !mutex_trylock(&filp->fd_lock)) {
            *fpp = filp;
            return 0;
        }
    }

    return ENFILE;
}

struct file_desc* get_filp(struct fproc* fp, int fd, rwlock_type_t lock_type)
{
    /* retrieve a file descriptor from fp's filp table and lock it */
    struct file_desc* filp = NULL;

    if (fd < 0 || fd >= NR_FILES) {
        err_code = EINVAL;
        return NULL;
    }
    filp = fp->filp[fd];

    if (!filp) {
        err_code = EBADF;
    } else {
        lock_filp(filp, lock_type);
    }
    return filp;
}
