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
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

struct eventfd_ctx {
    struct kref kref;
    struct wait_queue_head wq;
    u64 count;
    unsigned int flags;
};

#define EFD_SEMAPHORE (1 << 0)

static void eventfd_ctx_do_read(struct eventfd_ctx* ctx, u64* cnt)
{
    *cnt = (ctx->flags & EFD_SEMAPHORE) ? 1 : ctx->count;
    ctx->count -= *cnt;
}

static ssize_t eventfd_read(struct file_desc* filp, char* buf, size_t count,
                            loff_t* ppos, struct fproc* fp)
{
    struct eventfd_ctx* ctx = filp->fd_private_data;
    DECLARE_WAITQUEUE(wait, self);
    u64 ucount;
    ssize_t retval;

    if (count < sizeof(ucount)) {
        return -EINVAL;
    }

    retval = -EAGAIN;
    if (ctx->count > 0) {
        retval = sizeof(ucount);
    } else if (!(filp->fd_flags & O_NONBLOCK)) {
        waitqueue_add(&ctx->wq, &wait);
        retval = 0;

        for (;;) {
            if (ctx->count > 0) {
                retval = sizeof(ucount);
                break;
            }

            unlock_filp(filp);
            worker_wait();
            lock_filp(filp, RWL_WRITE);
        }

        waitqueue_remove(&ctx->wq, &wait);
    }

    if (retval > 0) {
        eventfd_ctx_do_read(ctx, &ucount);
        if (waitqueue_active(&ctx->wq)) {
            waitqueue_wakeup_all(&ctx->wq, (void*)EPOLLOUT);
        }
    }

    if (retval > 0 &&
        data_copy(fp->endpoint, buf, SELF, &ucount, sizeof(ucount))) {
        return -EFAULT;
    }

    return retval;
}

static ssize_t eventfd_write(struct file_desc* filp, const char* buf,
                             size_t count, loff_t* ppos, struct fproc* fp)
{
    struct eventfd_ctx* ctx = filp->fd_private_data;
    DECLARE_WAITQUEUE(wait, self);
    u64 ucount;
    ssize_t retval;

    if (count < sizeof(ucount)) {
        return -EINVAL;
    }

    retval = data_copy(SELF, &ucount, fp->endpoint, (void*)buf, sizeof(ucount));
    if (retval) {
        return -retval;
    }
    if (ucount == ULLONG_MAX) {
        return -EINVAL;
    }

    retval = -EAGAIN;
    if (ULLONG_MAX - ctx->count > ucount) {
        retval = sizeof(ucount);
    } else if (!(filp->fd_flags & O_NONBLOCK)) {
        waitqueue_add(&ctx->wq, &wait);
        retval = 0;

        for (;;) {
            if (ULLONG_MAX - ctx->count > ucount) {
                retval = sizeof(ucount);
                break;
            }

            unlock_filp(filp);
            worker_wait();
            lock_filp(filp, RWL_WRITE);
        }

        waitqueue_remove(&ctx->wq, &wait);
    }

    if (retval > 0) {
        ctx->count += ucount;
        if (waitqueue_active(&ctx->wq)) {
            waitqueue_wakeup_all(&ctx->wq, (void*)EPOLLIN);
        }
    }

    return retval;
}

static __poll_t eventfd_poll(struct file_desc* filp, __poll_t mask,
                             struct poll_table* wait, struct fproc* fp)
{
    struct eventfd_ctx* ctx = filp->fd_private_data;
    u64 count = ctx->count;

    poll_wait(filp, &ctx->wq, wait);

    mask = 0;
    if (count > 0) {
        mask |= EPOLLIN;
    }
    if (count == ULLONG_MAX) {
        mask |= EPOLLERR;
    }
    if (ULLONG_MAX - 1 > count) {
        mask |= EPOLLOUT;
    }

    return mask;
}

static void eventfd_free(struct kref* kref)
{
    struct eventfd_ctx* ctx = list_entry(kref, struct eventfd_ctx, kref);
    free(ctx);
}

static int eventfd_release(struct inode* pin, struct file_desc* filp)
{
    struct eventfd_ctx* ctx = filp->fd_private_data;

    waitqueue_wakeup_all(&ctx->wq, (void*)EPOLLHUP);
    kref_put(&ctx->kref, eventfd_free);

    return 0;
}

static const struct file_operations eventfd_fops = {
    .read = eventfd_read,
    .write = eventfd_write,
    .poll = eventfd_poll,
    .release = eventfd_release,
};

int do_eventfd(void)
{
    int count = self->msg_in.CNT;
    int flags = self->msg_in.FLAGS;
    int fd;
    struct eventfd_ctx* ctx;

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -ENOMEM;
    }

    kref_init(&ctx->kref);
    init_waitqueue_head(&ctx->wq);
    ctx->count = count;
    ctx->flags = flags;

    fd = anon_inode_get_fd(fproc, 0, &eventfd_fops, ctx,
                           O_RDWR | (flags & (O_NONBLOCK | O_CLOEXEC)));

    if (fd < 0) {
        free(ctx);
    }

    return fd;
}
