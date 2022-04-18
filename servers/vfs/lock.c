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
#include "errno.h"
#include <sys/syslimits.h>
#include <limits.h>
#include <sys/stat.h>
#include <asm/page.h>
#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"
#include "thread.h"

struct file_lock {
    short type;
    endpoint_t owner;
    struct inode* inode;
    off_t first;
    off_t last;
    struct wait_queue_head wq;
};

static struct file_lock lock_table[NR_LOCKS];

int do_lock(int fd, int req, void* arg)
{
    struct file_desc* filp;
    struct flock flock;
    off_t first, last;
    int lock_type;
    int conflict = FALSE, unlocking = FALSE;
    struct file_lock *empty, *flp, *flp2;
    DECLARE_WAITQUEUE(wait, self);
    int i, retval;

    assert(req == F_GETLK || req == F_SETLK || req == F_SETLKW);

    filp = fproc->files->filp[fd];
    assert(filp);

    if ((retval = data_copy(SELF, &flock, fproc->endpoint, arg,
                            sizeof(flock))) != OK)
        return -EINVAL;

    lock_type = flock.l_type;
    if (lock_type != F_UNLCK && lock_type != F_RDLCK && lock_type != F_WRLCK)
        return -EINVAL;
    if (req == F_GETLK && lock_type == F_UNLCK) return -EINVAL;
    if (!S_ISREG(filp->fd_inode->i_mode) && !S_ISBLK(filp->fd_inode->i_mode))
        return -EINVAL;
    if (req != F_GETLK && lock_type == F_RDLCK && !(filp->fd_mode & R_BIT))
        return -EBADF;
    if (req != F_GETLK && lock_type == F_WRLCK && !(filp->fd_mode & W_BIT))
        return -EBADF;

    switch (flock.l_whence) {
    case SEEK_SET:
        first = 0;
        break;
    case SEEK_CUR:
        first = filp->fd_pos;
        break;
    case SEEK_END:
        first = filp->fd_inode->i_size;
        break;
    default:
        return -EINVAL;
    }

    if (((long)flock.l_start > 0) && ((first + flock.l_start) < first))
        return -EINVAL;
    if (((long)flock.l_start < 0) && ((first + flock.l_start) > first))
        return -EINVAL;
    first = first + flock.l_start;
    last = first + flock.l_len - 1;
    if (flock.l_len == 0) last = INT_MAX;
    if (last < first) return -EINVAL;

restart:
    empty = NULL;
    for (flp = lock_table; flp < &lock_table[NR_LOCKS]; flp++) {
        if (flp->type == 0) {
            if (!empty) empty = flp;
            continue;
        }

        if (flp->inode != filp->fd_inode) continue;
        if (last < flp->first) continue;
        if (first > flp->last) continue;
        if (lock_type == F_RDLCK && flp->type == F_RDLCK) continue;
        if (lock_type != F_UNLCK && flp->owner == fproc->endpoint) continue;

        conflict = TRUE;
        if (req == F_GETLK) break;

        if (lock_type == F_RDLCK || lock_type == F_WRLCK) {
            if (req == F_SETLK) {
                return -EAGAIN;
            } else {
                waitqueue_add(&flp->wq, &wait);

                unlock_filp(filp);
                worker_wait(WT_BLOCKED_ON_FLOCK);
                lock_filp(filp, RWL_READ);

                waitqueue_remove(&flp->wq, &wait);

                goto restart;
            }
        }

        unlocking = TRUE;

        if (first <= flp->first && last >= flp->last) {
            flp->type = 0;
            continue;
        }

        if (first <= flp->first) {
            flp->first = last + 1;
            continue;
        }

        if (last >= flp->last) {
            flp->last = first - 1;
            continue;
        }

        for (i = 0; i < NR_LOCKS; i++)
            if (lock_table[i].type == 0) break;
        if (i == NR_LOCKS) return -ENOLCK;

        flp2 = &lock_table[i];
        flp2->type = flp->type;
        flp2->owner = flp->owner;
        flp2->inode = flp->inode;
        flp2->first = last + 1;
        flp2->last = flp->last;
        init_waitqueue_head(&flp2->wq);

        flp->last = first - 1;
    }

    if (unlocking) waitqueue_wakeup_all(&flp->wq, NULL);

    if (req == F_GETLK) {
        if (conflict) {
            flock.l_type = flp->type;
            flock.l_whence = SEEK_SET;
            flock.l_start = flp->first;
            flock.l_len = flp->last - flp->first + 1;
            flock.l_pid = get_epinfo(flp->owner, NULL, NULL);
        } else {
            flock.l_type = F_UNLCK;
        }

        if ((retval = data_copy(fproc->endpoint, arg, SELF, &flock,
                                sizeof(flock))) != OK)
            return -retval;

        return 0;
    }

    if (lock_type == F_UNLCK) return 0;

    if (!empty) return -ENOLCK;
    empty->type = lock_type;
    empty->owner = fproc->endpoint;
    empty->inode = filp->fd_inode;
    empty->first = first;
    empty->last = last;
    init_waitqueue_head(&empty->wq);

    return 0;
}
