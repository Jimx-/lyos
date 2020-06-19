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
