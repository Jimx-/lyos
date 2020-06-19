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
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <lyos/timer.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"

#define MAX_SELECTS 25

static struct select_entry {
    mutex_t lock;
    struct fproc* caller;
    endpoint_t endpoint;
    fd_set readfds, writefds, exceptfds;
    fd_set ready_readfds, ready_writefds, ready_exceptfds;
    fd_set *vir_readfds, *vir_writefds, *vir_exceptfds;
    struct file_desc* filps[OPEN_MAX];
    int type[OPEN_MAX];
    int nfds, nreadyfds;
    int error;
    char block, starting;
    clock_t expiry;
    struct timer_list timer;
} select_table[MAX_SELECTS];

static int select_request_char(struct file_desc* filp, int* ops, int block,
                               struct fproc* fp);
static int is_char_dev(struct file_desc* filp);

#define FDS_COPYIN 1
#define FDS_COPYOUT 2
static int copy_fdset(struct select_entry* entry, int nfds, int direction);
static int fd_getops(struct select_entry* entry, int fd);
static void fd_setfromops(struct select_entry* entry, int fd, int ops);
static void select_cancel(struct select_entry* entry);
static void select_cancel_filp(struct file_desc* filp);
static void select_lock_filp(struct file_desc* filp, int ops);
static int is_deferred(struct select_entry* entry);
static void update_status(struct file_desc* filp, int status);
static void tell_proc(struct select_entry* entry);
static void select_return(struct select_entry* entry);
static void select_timeout_check(struct timer_list* tp);

static struct fd_operation {
    int (*select_request)(struct file_desc* filp, int* ops, int block,
                          struct fproc* fp);
    int (*match)(struct file_desc* filp);
} fd_operations[] = {
    {select_request_char, is_char_dev},
};

#define FD_TYPES (sizeof(fd_operations) / sizeof(fd_operations[0]))

#define SEL_READ CDEV_SEL_RD
#define SEL_WRITE CDEV_SEL_WR
#define SEL_EXCEPT CDEV_SEL_EXC
#define SEL_NOTIFY CDEV_NOTIFY

#define lock_select_entry(entry) mutex_lock(&(entry)->lock)
#define unlock_select_entry(entry) mutex_unlock(&(entry)->lock)

void init_select()
{
    int slot;
    for (slot = 0; slot < MAX_SELECTS; slot++) {
        int i;
        for (i = 0; i < OPEN_MAX; i++)
            select_table[slot].filps[i] = NULL;
        mutex_init(&select_table[slot].lock, NULL);
    }
}

int do_select(MESSAGE* msg)
{
    int src = msg->source;
    int retval = 0;
    int nfds = msg->u.m_vfs_select.nfds;
    void* vtimeout = msg->u.m_vfs_select.timeout;
    struct timeval timeout;
    struct fproc* pcaller = vfs_endpt_proc(src);
    if (!pcaller) return EINVAL;

    if (nfds < 0 || nfds > OPEN_MAX) return EINVAL;
    int slot;
    for (slot = 0; slot < MAX_SELECTS; slot++) {
        if (select_table[slot].caller == NULL &&
            !mutex_trylock(&select_table[slot].lock))
            break;
    }
    if (slot == MAX_SELECTS) return ENOSPC;

    struct select_entry* entry = &select_table[slot];
    memset(entry, 0, sizeof(*entry));
    entry->caller = pcaller;
    entry->endpoint = src;
    entry->vir_readfds = msg->u.m_vfs_select.readfds;
    entry->vir_writefds = msg->u.m_vfs_select.writefds;
    entry->vir_exceptfds = msg->u.m_vfs_select.exceptfds;

    if ((retval = copy_fdset(entry, nfds, FDS_COPYIN)) != 0) {
        entry->caller = NULL;
        unlock_select_entry(entry);
        return retval;
    }

    int has_timeout = 0;
    if (vtimeout != 0) {
        retval = data_copy(SELF, &timeout, src, vtimeout, sizeof(timeout));

        if (retval == 0 && (timeout.tv_sec < 0 || timeout.tv_usec < 0 ||
                            timeout.tv_usec >= 1000000)) {
            retval = EINVAL;
        }
        if (retval) {
            entry->caller = NULL;
            unlock_select_entry(entry);
            return retval;
        }

        has_timeout = 1;
    }

    if (!has_timeout ||
        (has_timeout && (timeout.tv_usec > 0 || timeout.tv_sec > 0))) {
        entry->block = 1;
    } else {
        entry->block = 0;
    }
    entry->expiry = 0;

    entry->starting = TRUE;

    int fd;
    for (fd = 0; fd < nfds; fd++) {
        int ops = fd_getops(entry, fd);
        if (!ops) continue;

        struct file_desc* filp;
        filp = entry->filps[fd] = get_filp(pcaller, fd, RWL_READ);
        if (!filp && errno != EIO) {
            entry->error = EBADF;
            break;
        }

        entry->type[fd] = -1;
        if (!filp) continue;
        int type;
        for (type = 0; type < FD_TYPES; type++) {
            if (fd_operations[type].match(filp)) {
                entry->type[fd] = type;
                entry->nfds = fd + 1;
                filp->fd_selectors++;
                break;
            }
        }
        unlock_filp(filp);

        if (entry->type[fd] == -1) {
            entry->error = EBADF;
            break;
        }
    }

    if (entry->error != 0) {
        select_cancel(entry);
        entry->caller = NULL;
        retval = entry->error;

        unlock_select_entry(entry);
        return retval;
    }

    for (fd = 0; fd < nfds; fd++) {
        int ops = fd_getops(entry, fd);
        if (!ops) continue;

        struct file_desc* filp = entry->filps[fd];
        if (filp == NULL) {
            fd_setfromops(entry, fd, SEL_READ | SEL_WRITE);
            continue;
        }
        if ((ops & SEL_READ) && (filp->fd_mode & R_BIT)) {
            fd_setfromops(entry, fd, SEL_READ);
            ops &= ~SEL_READ;
        }
        if ((ops & SEL_WRITE) && (filp->fd_mode & W_BIT)) {
            fd_setfromops(entry, fd, SEL_WRITE);
            ops &= ~SEL_WRITE;
        }

        if ((filp->fd_select_ops & ops) != ops) {
            int newops;

            newops = filp->fd_select_ops | ops;
            select_lock_filp(filp, newops);
            retval = fd_operations[entry->type[fd]].select_request(
                filp, &newops, entry->block, pcaller);
            unlock_filp(filp);
            if (retval && retval != SUSPEND) {
                entry->error = retval;
                break;
            }

            if (newops & ops) fd_setfromops(entry, fd, newops);
        }
    }

    entry->starting = FALSE;

    if ((entry->nreadyfds > 0 || entry->error != 0 || !entry->block) &&
        !is_deferred(entry)) {
        if (entry->error)
            retval = entry->error;
        else {
            retval = copy_fdset(entry, entry->nfds, FDS_COPYOUT);
        }

        select_cancel(entry);
        entry->caller = NULL;

        if (!retval) retval = entry->nreadyfds;

        unlock_select_entry(entry);
        return retval;
    }

    if (has_timeout && entry->block) {
        clock_t ticks;
        ticks = timeout.tv_sec * system_hz +
                (timeout.tv_usec * system_hz + 1000000L - 1) / 1000000L;
        entry->expiry = ticks;
        set_timer(&entry->timer, ticks, select_timeout_check, slot);
    }

    unlock_select_entry(entry);
    return SUSPEND;
}

static void select_timeout_check(struct timer_list* tp)
{
    struct select_entry* entry;
    int slot = tp->arg;
    if (slot < 0 || slot >= MAX_SELECTS) return;

    entry = &select_table[slot];
    if (entry->caller == NULL) return;
    if (entry->expiry == 0) return;

    entry->expiry = 0;
    if (!is_deferred(entry)) {
        select_return(entry);
    } else {
        entry->block = 0;
    }
}

static int select_filter(struct file_desc* filp, int* ops, int block)
{
    int rops = *ops;
    *ops = 0;

    if (!block && !(filp->fd_select_flags & (SFL_UPDATE | SFL_BUSY)) &&
        (filp->fd_select_flags & SFL_BLOCKED)) {
        if ((rops & SEL_READ) && (filp->fd_select_flags & SFL_RD_BLOCK))
            rops &= ~SEL_READ;
        if ((rops & SEL_WRITE) && (filp->fd_select_flags & SFL_WR_BLOCK))
            rops &= ~SEL_WRITE;
        if ((rops & SEL_EXCEPT) && (filp->fd_select_flags & SFL_EXC_BLOCK))
            rops &= ~SEL_EXCEPT;
        if (!(rops & (SEL_READ | SEL_WRITE | SEL_EXCEPT))) {
            return 0;
        }
    }

    filp->fd_select_flags |= SFL_UPDATE;
    if (block) {
        rops |= SEL_NOTIFY;
        if (rops & SEL_READ) filp->fd_select_flags |= SFL_RD_BLOCK;
        if (rops & SEL_WRITE) filp->fd_select_flags |= SFL_WR_BLOCK;
        if (rops & SEL_EXCEPT) filp->fd_select_flags |= SFL_EXC_BLOCK;
    }

    if (filp->fd_select_flags & SFL_BUSY) return SUSPEND;
    return rops;
}

static int select_request_char(struct file_desc* filp, int* ops, int block,
                               struct fproc* fp)
{
    dev_t dev = filp->fd_inode->i_specdev;

    filp->fd_select_dev = dev;

    int rops = select_filter(filp, ops, block);
    if (rops <= 0) return rops;

    struct cdmap* pm = &cdmap[MAJOR(dev)];
    if (pm->select_busy) return SUSPEND;

    filp->fd_select_flags &= ~SFL_UPDATE;
    int retval = cdev_select(dev, rops, fp);
    if (retval) return retval;

    pm->select_busy = 1;
    pm->select_filp = filp;
    filp->fd_select_flags |= SFL_BUSY;

    return 0;
}

static int is_char_dev(struct file_desc* filp)
{
    return filp && filp->fd_inode && (filp->fd_inode->i_mode & I_CHAR_SPECIAL);
}

static int copy_fdset(struct select_entry* entry, int nfds, int direction)
{
    if (nfds < 0 || nfds > OPEN_MAX) return EINVAL;
    int fdset_size = (size_t)(_howmany(nfds, NFDBITS) * sizeof(fd_mask));
    int retval;

    int copyin = (direction == FDS_COPYIN);
    endpoint_t src_ep, dst_ep;
    fd_set *src_fds, *dst_fds;

    src_ep = copyin ? entry->endpoint : SELF;
    dst_ep = copyin ? SELF : entry->endpoint;

    src_fds = copyin ? entry->vir_readfds : &entry->ready_readfds;
    dst_fds = copyin ? &entry->readfds : entry->vir_readfds;
    if (entry->vir_readfds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    src_fds = copyin ? entry->vir_writefds : &entry->ready_writefds;
    dst_fds = copyin ? &entry->writefds : entry->vir_writefds;
    if (entry->vir_writefds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    src_fds = copyin ? entry->vir_exceptfds : &entry->ready_exceptfds;
    dst_fds = copyin ? &entry->exceptfds : entry->vir_exceptfds;
    if (entry->vir_exceptfds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    return 0;
}

static int fd_getops(struct select_entry* entry, int fd)
{
    int ops = 0;

    if (FD_ISSET(fd, &entry->readfds)) ops |= SEL_READ;
    if (FD_ISSET(fd, &entry->writefds)) ops |= SEL_WRITE;
    if (FD_ISSET(fd, &entry->exceptfds)) ops |= SEL_EXCEPT;

    return ops;
}

static void fd_setfromops(struct select_entry* entry, int fd, int ops)
{
    if ((ops & SEL_READ) && entry->vir_readfds &&
        FD_ISSET(fd, &entry->readfds) && !FD_ISSET(fd, &entry->ready_readfds)) {
        FD_SET(fd, &entry->ready_readfds);
        entry->nreadyfds++;
    }

    if ((ops & SEL_WRITE) && entry->vir_writefds &&
        FD_ISSET(fd, &entry->writefds) &&
        !FD_ISSET(fd, &entry->ready_writefds)) {
        FD_SET(fd, &entry->ready_writefds);
        entry->nreadyfds++;
    }

    if ((ops & SEL_EXCEPT) && entry->vir_exceptfds &&
        FD_ISSET(fd, &entry->exceptfds) &&
        !FD_ISSET(fd, &entry->ready_exceptfds)) {
        FD_SET(fd, &entry->ready_exceptfds);
        entry->nreadyfds++;
    }
}

static void select_cancel(struct select_entry* entry)
{
    int fd;
    struct file_desc* filp;
    for (fd = 0; fd < entry->nfds; fd++) {
        if ((filp = entry->filps[fd]) == NULL) {
            continue;
        }
        entry->filps[fd] = NULL;
        select_cancel_filp(filp);
    }

    if (entry->expiry) {
        timer_remove(&entry->timer);
        entry->expiry = 0;
    }

    entry->caller = NULL;
}

static void select_cancel_filp(struct file_desc* filp)
{
    filp->fd_selectors--;
    if (filp->fd_selectors == 0) {
        filp->fd_select_flags = 0;
        filp->fd_select_ops = 0;
    }
}

static void select_lock_filp(struct file_desc* filp, int ops)
{
    rwlock_type_t lock_type = RWL_READ;

    if (ops & (SEL_EXCEPT | SEL_WRITE)) {
        lock_type = RWL_WRITE;
    }

    lock_filp(filp, lock_type);
}

static int is_deferred(struct select_entry* entry)
{
    int fd;
    if (entry->starting) return TRUE;
    for (fd = 0; fd < entry->nfds; fd++) {
        struct file_desc* filp = entry->filps[fd];
        if (!filp) continue;
        if (filp->fd_select_flags & (SFL_BUSY | SFL_UPDATE)) return TRUE;
    }

    return FALSE;
}

static void update_status(struct file_desc* filp, int status)
{
    int slot, found;
    for (slot = 0; slot < MAX_SELECTS; slot++) {
        struct select_entry* entry = &select_table[slot];
        if (entry->caller == NULL) continue;

        found = 0;
        int fd;
        for (fd = 0; fd < entry->nfds; fd++) {
            if (entry->filps[fd] != filp) continue;

            if (status < 0) {
                entry->error = status;
            } else {
                fd_setfromops(entry, fd, status);
            }
            found = 1;
        }

        if (found) {
            tell_proc(entry);
        }
    }
}

static void tell_proc(struct select_entry* entry)
{
    if ((entry->nreadyfds > 0 || entry->error != 0 || !entry->block) &&
        !is_deferred(entry)) {
        select_return(entry);
    }
}

static void select_return(struct select_entry* entry)
{
    select_cancel(entry);

    int retval = 0;
    if (entry->error) {
        retval = entry->error;
    } else {
        retval = copy_fdset(entry, entry->nfds, FDS_COPYOUT);
    }

    if (!retval) {
        retval = entry->nreadyfds;
    }

    MESSAGE mess;
    memset(&mess, 0, sizeof(mess));
    mess.type = SYSCALL_RET;
    mess.RETVAL = retval;
    /* revive_proc(entry->endpoint, &mess); */
}

static void select_reply1(struct file_desc* filp, int status)
{
    filp->fd_select_flags &= ~SFL_BUSY;
    if (!(filp->fd_select_flags & (SFL_UPDATE | SFL_BLOCKED))) {
        filp->fd_select_ops = 0;
    } else if (status > 0 && !(filp->fd_select_flags & SFL_UPDATE)) {
        filp->fd_select_ops &= ~status;
    }

    if (!(status == 0 && (filp->fd_select_flags & SFL_BLOCKED))) {
        if (status > 0) {
            if (status & SEL_READ) {
                filp->fd_select_flags &= ~SFL_RD_BLOCK;
            }
            if (status & SEL_WRITE) {
                filp->fd_select_flags &= ~SFL_WR_BLOCK;
            }
            if (status & SEL_EXCEPT) {
                filp->fd_select_flags &= ~SFL_EXC_BLOCK;
            }
        } else if (status < 0) {
            filp->fd_select_flags &= ~SFL_BLOCKED;
        }
    }

    update_status(filp, status);
}

void do_select_cdev_reply1(endpoint_t driver_ep, dev_t minor, int status)
{
    /* handle the inital select reply from device driver */
    struct cdmap* pm = cdev_lookup_by_endpoint(driver_ep);
    if (!pm) return;
    if (!pm->select_busy) return;

    struct file_desc* filp = pm->select_filp;
    pm->select_busy = 0;
    pm->select_filp = NULL;

    if (filp) {
        select_reply1(filp, status);
    }
}

static void select_reply2(int is_char, dev_t dev, int status)
{
    int slot;
    for (slot = 0; slot < MAX_SELECTS; slot++) {
        struct select_entry* entry = &select_table[slot];
        if (!entry->caller) continue;

        int found = 0;
        int fd;
        for (fd = 0; fd < entry->nfds; fd++) {
            struct file_desc* filp = entry->filps[fd];
            if (!filp) continue;
            if (is_char && !is_char_dev(filp)) continue;
            if (filp->fd_select_dev != dev) continue;

            if (status > 0) {
                if (!(filp->fd_select_flags & SFL_UPDATE)) {
                    filp->fd_select_ops &= ~status;
                }

                if (status & SEL_READ) {
                    filp->fd_select_flags &= ~SFL_RD_BLOCK;
                }
                if (status & SEL_WRITE) {
                    filp->fd_select_flags &= ~SFL_WR_BLOCK;
                }
                if (status & SEL_EXCEPT) {
                    filp->fd_select_flags &= ~SFL_EXC_BLOCK;
                }

                fd_setfromops(entry, fd, status);
            } else {
                filp->fd_select_flags &= ~SFL_BLOCKED;
                entry->error = status;
            }
            found = 1;
        }

        if (found) {
            tell_proc(entry);
        }
    }
}

void do_select_cdev_reply2(endpoint_t driver_ep, dev_t minor, int status)
{
    /* handle the secondary select reply when the request is blocked and an
     * operation becomes ready */
    struct cdmap* pm = cdev_lookup_by_endpoint(driver_ep);
    if (!pm) return;

    dev_t major = pm - cdmap;
    dev_t dev = MAKE_DEV(major, minor);
    select_reply2(1, dev, status);
}
