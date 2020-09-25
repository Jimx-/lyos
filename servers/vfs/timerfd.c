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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/kref.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <limits.h>
#include <sys/time.h>
#include <lyos/timer.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

#define NSEC_PER_SEC (1000000000ULL)

static void timerfd_tmrproc(struct timer_list* tp);

struct timerfd_ctx {
    struct wait_queue_head wq;
    int clock_id;
    struct timer_list timer;
    u64 ticks;
    clock_t tintv;
};

static clock_t timespec_to_ticks(const struct timespec* ts)
{
    return (clock_t)ts->tv_sec * system_hz +
           (clock_t)(((u64)ts->tv_nsec * system_hz + NSEC_PER_SEC - 1) /
                     NSEC_PER_SEC);
}

static void ticks_to_timespec(clock_t ticks, struct timespec* ts)
{
    ts->tv_sec = ticks / system_hz;
    ts->tv_nsec =
        (unsigned long)((u64)(ticks % system_hz) * NSEC_PER_SEC / system_hz);
}

static void timerfd_triggered(struct timerfd_ctx* ctx)
{
    ctx->ticks++;
    waitqueue_wakeup_all(&ctx->wq, (void*)EPOLLIN);

    if (ctx->tintv) {
        set_timer(&ctx->timer, ctx->tintv, timerfd_tmrproc, NULL);
    }
}

static void timerfd_tmrproc(struct timer_list* tp)
{
    struct timerfd_ctx* ctx = list_entry(tp, struct timerfd_ctx, timer);
    timerfd_triggered(ctx);
}

static int timerfd_setup(struct timerfd_ctx* ctx, int flags,
                         const struct itimerspec* ktmr)
{
    struct timespec now;
    clock_t texp, ticks_now;
    int retval;

    texp = timespec_to_ticks(&ktmr->it_value);

    ctx->ticks = 0;
    ctx->tintv = timespec_to_ticks(&ktmr->it_interval);

    if (texp > 0) {
        if (flags & TFD_TIMER_ABSTIME) {
            retval = clock_gettime(ctx->clock_id, &now);
            if (retval < 0) return errno;

            ticks_now = timespec_to_ticks(&now);

            if (texp > ticks_now) {
                texp -= ticks_now;
            } else {
                texp = 0;
            }
        }

        if (texp > 0) {
            set_timer(&ctx->timer, texp, timerfd_tmrproc, NULL);
        } else {
            timerfd_triggered(ctx);
        }
    }

    return 0;
}

static ssize_t timerfd_read(struct file_desc* filp, char* buf, size_t count,
                            loff_t* ppos, struct fproc* fp)
{
    struct timerfd_ctx* ctx = filp->fd_private_data;
    DECLARE_WAITQUEUE(wait, self);
    int retval;
    u64 ticks = 0;

    if (count < sizeof(ticks)) {
        return -EINVAL;
    }

    if (filp->fd_mode & O_NONBLOCK) {
        retval = -EAGAIN;
    } else {
        if (!ctx->ticks) {
            waitqueue_add(&ctx->wq, &wait);
            unlock_filp(filp);
            worker_wait();
            lock_filp(filp, RWL_READ);
            waitqueue_remove(&ctx->wq, &wait);
        }

        retval = 0;
    }

    if (ctx->ticks) {
        ticks = ctx->ticks;
        ctx->ticks = 0;
    }

    if (ticks) {
        retval = data_copy(fp->endpoint, buf, SELF, &ticks, sizeof(ticks));
        if (retval) {
            return -retval;
        }

        retval = sizeof(ticks);
    }

    return retval;
}

static __poll_t timerfd_poll(struct file_desc* filp, __poll_t mask,
                             struct poll_table* wait, struct fproc* fp)
{
    struct timerfd_ctx* ctx = filp->fd_private_data;

    poll_wait(filp, &ctx->wq, wait);

    mask = 0;
    if (ctx->ticks) {
        mask |= EPOLLIN;
    }

    return mask;
}

static int timerfd_release(struct inode* pin, struct file_desc* filp)
{
    struct timerfd_ctx* ctx = filp->fd_private_data;

    cancel_timer(&ctx->timer);
    free(ctx);

    return 0;
}

static const struct file_operations timerfd_fops = {
    .read = timerfd_read,
    .poll = timerfd_poll,
    .release = timerfd_release,
};

int do_timerfd_create(void)
{
    int clock_id = self->msg_in.u.m_vfs_timerfd.clock_id;
    int flags = self->msg_in.u.m_vfs_timerfd.flags;
    struct timerfd_ctx* ctx;
    int fd;

    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
        return -EINVAL;
    }

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -ENOMEM;
    }

    memset(ctx, 0, sizeof(*ctx));
    init_waitqueue_head(&ctx->wq);
    init_timer(&ctx->timer);
    ctx->clock_id = clock_id;

    fd = anon_inode_get_fd(fproc, 0, &timerfd_fops, ctx,
                           O_RDWR | (flags & (O_NONBLOCK | O_CLOEXEC)));

    if (fd < 0) {
        free(ctx);
    }

    return fd;
}

int do_timerfd_settime(void)
{
    endpoint_t src = self->msg_in.source;
    int fd = self->msg_in.u.m_vfs_timerfd.fd;
    int flags = self->msg_in.u.m_vfs_timerfd.flags;
    void* old_value_ptr = self->msg_in.u.m_vfs_timerfd.old_value;
    void* new_value_ptr = self->msg_in.u.m_vfs_timerfd.new_value;
    struct itimerspec old_value, new_value;
    struct file_desc* filp;
    struct timerfd_ctx* ctx;
    int retval;

    retval = data_copy(SELF, &new_value, src, new_value_ptr, sizeof(new_value));
    if (retval) return retval;

    filp = get_filp(fproc, fd, RWL_WRITE);
    if (!filp) return EBADF;

    if (filp->fd_fops != &timerfd_fops) {
        unlock_filp(filp);
        return EINVAL;
    }

    ctx = filp->fd_private_data;

    ticks_to_timespec(timer_expires_remaining(&ctx->timer),
                      &old_value.it_value);
    ticks_to_timespec(ctx->tintv, &old_value.it_interval);

    retval = timerfd_setup(ctx, flags, &new_value);
    if (retval) goto err;

    if (old_value_ptr) {
        retval =
            data_copy(src, old_value_ptr, SELF, &old_value, sizeof(old_value));
    }

err:
    unlock_filp(filp);
    return retval;
}

int do_timerfd_gettime(void)
{
    endpoint_t src = self->msg_in.source;
    int fd = self->msg_in.u.m_vfs_timerfd.fd;
    void* old_value_ptr = self->msg_in.u.m_vfs_timerfd.old_value;
    struct itimerspec old_value;
    struct file_desc* filp;
    struct timerfd_ctx* ctx;
    int retval;

    filp = get_filp(fproc, fd, RWL_READ);
    if (!filp) return EBADF;

    if (filp->fd_fops != &timerfd_fops) {
        unlock_filp(filp);
        return EINVAL;
    }

    ctx = filp->fd_private_data;

    ticks_to_timespec(timer_expires_remaining(&ctx->timer),
                      &old_value.it_value);
    ticks_to_timespec(ctx->tintv, &old_value.it_interval);

    retval = data_copy(src, old_value_ptr, SELF, &old_value, sizeof(old_value));

    unlock_filp(filp);
    return retval;
}
