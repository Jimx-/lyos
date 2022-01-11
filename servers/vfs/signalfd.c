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
#include <errno.h>
#include <sys/syslimits.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include <poll.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "global.h"
#include "poll.h"

struct signalfd_ctx {
    sigset_t sigmask;
};

static void signalfd_recv(MESSAGE* msg)
{
    self->recv_from = TASK_PM;
    self->msg_driver = msg;

    worker_wait(WT_BLOCKED_ON_DRV_MSG);
    self->recv_from = NO_TASK;
}

static int request_signalfd_dequeue(struct signalfd_ctx* ctx, void* buf,
                                    struct fproc* fp, int notify)
{
    MESSAGE msg;
    msg.type = PM_SIGNALFD_DEQUEUE;
    msg.u.m_vfs_pm_signalfd.endpoint = fp->endpoint;
    msg.u.m_vfs_pm_signalfd.sigmask = ctx->sigmask;
    msg.u.m_vfs_pm_signalfd.buf = buf;
    msg.u.m_vfs_pm_signalfd.notify = notify;

    if (asyncsend3(TASK_PM, &msg, 0)) {
        panic("vfs: signalfd_dequeue send message failed");
    }

    signalfd_recv(&msg);

    return msg.u.m_pm_vfs_signalfd_reply.status;
}

static int request_signalfd_get_pending(struct signalfd_ctx* ctx,
                                        struct fproc* fp, int notify)
{
    MESSAGE msg;
    msg.type = PM_SIGNALFD_GETNEXT;
    msg.u.m_vfs_pm_signalfd.endpoint = fp->endpoint;
    msg.u.m_vfs_pm_signalfd.sigmask = ctx->sigmask;
    msg.u.m_vfs_pm_signalfd.notify = notify;

    if (asyncsend3(TASK_PM, &msg, 0)) {
        panic("vfs: signalfd_dequeue send message failed");
    }

    signalfd_recv(&msg);

    return msg.u.m_pm_vfs_signalfd_reply.status;
}

static ssize_t signalfd_dequeue(struct file_desc* filp, void* buf,
                                struct fproc* fp, int nonblock)
{
    struct signalfd_ctx* ctx = filp->fd_private_data;
    int retval;
    DECLARE_WAITQUEUE(wait, self);

    retval = request_signalfd_dequeue(ctx, buf, fp, !nonblock);

    if (!retval) {
        if (nonblock) {
            return -EAGAIN;
        }
    } else {
        return retval;
    }

    waitqueue_add(&fp->signalfd_wq, &wait);
    for (;;) {
        unlock_filp(filp);
        worker_wait(WT_BLOCKED_ON_SFD);
        lock_filp(filp, RWL_READ);

        retval = request_signalfd_dequeue(ctx, buf, fp, !nonblock);

        if (retval) break;
    }
    waitqueue_remove(&fp->signalfd_wq, &wait);

    return retval;
}

static ssize_t signalfd_read(struct file_desc* filp, char* buf, size_t count,
                             loff_t* ppos, struct fproc* fp)
{
    int nonblock = filp->fd_flags & O_NONBLOCK;
    int retval;
    ssize_t total;

    count /= sizeof(struct signalfd_siginfo);
    if (!count) return -EINVAL;

    total = 0;
    do {
        retval = signalfd_dequeue(filp, buf, fp, nonblock);
        if (retval <= 0) {
            break;
        }

        total += sizeof(struct signalfd_siginfo);
        buf += sizeof(struct signalfd_siginfo);
    } while (--count);

    return total ? total : retval;
}

static __poll_t signalfd_poll(struct file_desc* filp, __poll_t mask,
                              struct poll_table* wait, struct fproc* fp)
{
    struct signalfd_ctx* ctx = filp->fd_private_data;
    int signo;

    signo = request_signalfd_get_pending(ctx, fp, TRUE /* notify */);
    if (signo < 0) {
        return 0;
    }

    poll_wait(filp, &fp->signalfd_wq, wait);

    mask = 0;
    if (signo > 0) {
        mask |= EPOLLIN;
    }

    return mask;
}

static int signalfd_release(struct inode* pin, struct file_desc* filp,
                            int may_block)
{
    if (waitqueue_active(&fproc->signalfd_wq)) {
        waitqueue_wakeup_all(&fproc->signalfd_wq, (void*)(EPOLLHUP | POLLFREE));
    }

    free(filp->fd_private_data);
    return 0;
}

static const struct file_operations signalfd_fops = {
    .read = signalfd_read,
    .poll = signalfd_poll,
    .release = signalfd_release,
};

static int get_signalfd_flags(int sflags)
{
    int flags = 0;

    if (sflags & SFD_CLOEXEC) flags |= O_CLOEXEC;
    if (sflags & SFD_NONBLOCK) flags |= O_NONBLOCK;

    return flags;
}

int do_signalfd(void)
{
    int fd = self->msg_in.u.m_vfs_signalfd.fd;
    sigset_t mask = (sigset_t)self->msg_in.u.m_vfs_signalfd.mask;
    int flags = self->msg_in.u.m_vfs_signalfd.flags;
    struct signalfd_ctx* ctx;
    struct file_desc* filp;

    if (flags & ~(SFD_CLOEXEC | SFD_NONBLOCK)) return -EINVAL;
    flags = get_signalfd_flags(flags);

    sigdelset(&mask, SIGKILL);
    sigdelset(&mask, SIGSTOP);

    if (fd == -1) {
        ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            return -ENOMEM;
        }

        ctx->sigmask = mask;

        fd = anon_inode_get_fd(fproc, 0, &signalfd_fops, ctx,
                               O_RDWR | (flags & (O_NONBLOCK | O_CLOEXEC)));

        if (fd < 0) {
            free(ctx);
        }
    } else {
        filp = get_filp(fproc, fd, RWL_WRITE);
        if (!filp) {
            return -EBADF;
        }

        ctx = filp->fd_private_data;
        if (filp->fd_fops != &signalfd_fops) {
            unlock_filp(filp);
            return -EINVAL;
        }

        ctx->sigmask = mask;

        waitqueue_wakeup_all(&fproc->signalfd_wq, NULL);

        unlock_filp(filp);
    }

    return fd;
}

static void signalfd_reply_generic(MESSAGE* msg)
{
    endpoint_t endpoint = msg->u.m_pm_vfs_signalfd_reply.endpoint;

    struct fproc* fp = vfs_endpt_proc(endpoint);
    if (fp == NULL) return;

    struct worker_thread* worker = fp->worker;
    if (worker != NULL && worker->msg_driver != NULL &&
        worker->blocked_on == WT_BLOCKED_ON_DRV_MSG &&
        worker->recv_from == msg->source) {
        *worker->msg_driver = *msg;
        worker->msg_driver = NULL;
        worker_wake(worker);
    }
}

static void signalfd_poll_notify(MESSAGE* msg)
{
    endpoint_t endpoint = msg->u.m_pm_vfs_signalfd_reply.endpoint;
    struct fproc* fp = vfs_endpt_proc(endpoint);
    if (fp == NULL) return;

    waitqueue_wakeup_all(&fp->signalfd_wq, (void*)EPOLLIN);
}

int do_pm_signalfd_reply(MESSAGE* msg)
{
    switch (msg->type) {
    case PM_SIGNALFD_REPLY:
        signalfd_reply_generic(msg);
        break;
    case PM_SIGNALFD_POLL_NOTIFY:
        signalfd_poll_notify(msg);
        break;
    }

    return 0;
}
