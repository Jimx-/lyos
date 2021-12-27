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
#include <sys/time.h>
#include <lyos/timer.h>
#include <lyos/eventpoll.h>
#include <poll.h>

#include "types.h"
#include "path.h"
#include "proto.h"
#include "fcntl.h"
#include "global.h"
#include "poll.h"

#define MAX_INLINE_POLL_ENTRIES 16

struct poll_table_entry {
    struct file_desc* filp;
    __poll_t mask;
    struct wait_queue_head* wq;
    struct wait_queue_entry wait;
};

struct poll_wqueues {
    struct poll_table pt;
    struct worker_thread* polling_worker;
    int triggered;
    int inline_index;
    struct poll_table_entry inline_entries[MAX_INLINE_POLL_ENTRIES];
};

struct select_fdset {
    endpoint_t endpoint;
    fd_set readfds, writefds, exceptfds;
    fd_set ready_readfds, ready_writefds, ready_exceptfds;
    fd_set *vir_readfds, *vir_writefds, *vir_exceptfds;
    size_t nfds, nreadyfds;
};

struct select_timer_cb_data {
    int expired;
    struct worker_thread* worker;
};

static void __pollwait(struct file_desc*, struct wait_queue_head*,
                       struct poll_table*);

#define FDS_COPYIN  1
#define FDS_COPYOUT 2
static int copy_fdset(struct select_fdset* fdset, int nfds, int direction);
static int fd_getops(struct select_fdset* fdset, int fd);
static void fd_setfromops(struct select_fdset* fdset, int fd, int ops);
static void select_timeout_check(struct timer_list* tp);

#define SEL_READ   0x01
#define SEL_WRITE  0x02
#define SEL_EXCEPT 0x04

void init_select(void) {}

static void poll_initwait(struct poll_wqueues* pwq)
{
    pwq->pt.qproc = __pollwait;
    pwq->polling_worker = self;
    pwq->triggered = FALSE;
    pwq->inline_index = 0;
}

static void poll_freewait(struct poll_wqueues* pwq)
{
    struct poll_table_entry* entry;

    for (entry = pwq->inline_entries;
         entry < &pwq->inline_entries[pwq->inline_index]; entry++) {
        waitqueue_remove(entry->wq, &entry->wait);
    }
}

static struct poll_table_entry* poll_get_entry(struct poll_wqueues* pwq)
{
    if (pwq->inline_index < MAX_INLINE_POLL_ENTRIES) {
        return &pwq->inline_entries[pwq->inline_index++];
    }

    return NULL;
}

static void wait_set_key(struct poll_table* wait, int ops)
{
#define POLLIN_SET  (EPOLLRDNORM | EPOLLRDBAND | EPOLLIN | EPOLLHUP | EPOLLERR)
#define POLLOUT_SET (EPOLLWRBAND | EPOLLWRNORM | EPOLLOUT | EPOLLERR)
#define POLLEX_SET  (EPOLLPRI)

    wait->mask = POLLEX_SET;
    if (ops & SEL_READ) {
        wait->mask |= POLLIN_SET;
    }
    if (ops & SEL_WRITE) {
        wait->mask |= POLLOUT_SET;
    }
}

int do_select(void)
{
    int src = self->msg_in.source;
    int retval = 0;
    int nfds = self->msg_in.u.m_vfs_select.nfds;
    struct poll_wqueues pwq;
    struct poll_table* wait = &pwq.pt;
    void* vtimeout = self->msg_in.u.m_vfs_select.timeout;
    struct timeval timeout;
    struct timer_list timer;
    struct select_timer_cb_data timer_cb;
    int timed_out = 0;

    if (nfds < 0 || nfds > OPEN_MAX) return -EINVAL;

    struct select_fdset fdset;
    memset(&fdset, 0, sizeof(fdset));
    fdset.endpoint = src;
    fdset.vir_readfds = self->msg_in.u.m_vfs_select.readfds;
    fdset.vir_writefds = self->msg_in.u.m_vfs_select.writefds;
    fdset.vir_exceptfds = self->msg_in.u.m_vfs_select.exceptfds;
    fdset.nfds = nfds;

    poll_initwait(&pwq);

    if ((retval = copy_fdset(&fdset, nfds, FDS_COPYIN)) != 0) {
        return -retval;
    }

    int has_timeout = 0;
    if (vtimeout != 0) {
        retval = data_copy(SELF, &timeout, src, vtimeout, sizeof(timeout));

        if (retval == 0 && (timeout.tv_sec < 0 || timeout.tv_usec < 0 ||
                            timeout.tv_usec >= 1000000)) {
            retval = EINVAL;
        }
        if (retval) {
            return -retval;
        }

        has_timeout = 1;
    }

    if (has_timeout && !timeout.tv_usec && !timeout.tv_sec) {
        wait->qproc = NULL;
        timed_out = TRUE;
    }

    retval = 0;
    for (;;) {
        int fd;

        // poll each fd
        for (fd = 0; fd < nfds; fd++) {
            int ops, res_ops;
            struct file_desc* filp;
            rwlock_type_t filp_lock_type = RWL_READ;
            __poll_t mask;

            res_ops = 0;

            ops = fd_getops(&fdset, fd);

            if (!ops) continue;

            if (ops & (SEL_WRITE | SEL_EXCEPT)) {
                filp_lock_type = RWL_WRITE;
            }

            filp = get_filp(fproc, fd, filp_lock_type);
            if (!filp) continue;

            wait_set_key(wait, ops);
            mask = vfs_poll(
                filp, wait->mask | (timed_out ? 0 : POLL_NOTIFY) | POLL_ONESHOT,
                wait, fproc);

            unlock_filp(filp);

            if ((mask & POLLIN_SET) && (ops & SEL_READ)) {
                res_ops |= SEL_READ;
                wait->qproc = NULL;
            }
            if ((mask & POLLOUT_SET) && (ops & SEL_WRITE)) {
                res_ops |= SEL_WRITE;
                wait->qproc = NULL;
            }
            if ((mask & POLLEX_SET) && (ops & SEL_EXCEPT)) {
                res_ops |= SEL_EXCEPT;
                wait->qproc = NULL;
            }

            fd_setfromops(&fdset, fd, res_ops);
        }

        wait->qproc = NULL;

        // got something
        if (fdset.nreadyfds || timed_out) {
            break;
        }

        // prepare to sleep
        if (!pwq.triggered) {
            if (has_timeout) {
                timer_cb.expired = FALSE;
                timer_cb.worker = self;

                clock_t ticks;
                ticks = timeout.tv_sec * system_hz +
                        (timeout.tv_usec * system_hz + 1000000L - 1) / 1000000L;
                set_timer(&timer, ticks, select_timeout_check, &timer_cb);
            }

            worker_wait(WT_BLOCKED_ON_POLL);

            if (has_timeout) {
                timed_out = timer_cb.expired;
                if (!timed_out) {
                    cancel_timer(&timer);
                }
            }
        }
    }

    poll_freewait(&pwq);

    if (fdset.nreadyfds > 0) {
        retval = copy_fdset(&fdset, fdset.nfds, FDS_COPYOUT);

        if (!retval)
            retval = fdset.nreadyfds;
        else
            retval = -retval;
    }

    return retval;
}

static void select_timeout_check(struct timer_list* tp)
{
    struct select_timer_cb_data* timer_cb = tp->arg;
    struct worker_thread* worker = timer_cb->worker;

    timer_cb->expired = TRUE;

    if (worker && worker->blocked_on == WT_BLOCKED_ON_POLL) worker_wake(worker);
}

static int copy_fdset(struct select_fdset* fdset, int nfds, int direction)
{
    if (nfds < 0 || nfds > OPEN_MAX) return EINVAL;
    int fdset_size = (size_t)(_howmany(nfds, NFDBITS) * sizeof(fd_mask));
    int retval;

    int copyin = (direction == FDS_COPYIN);
    endpoint_t src_ep, dst_ep;
    fd_set *src_fds, *dst_fds;

    src_ep = copyin ? fdset->endpoint : SELF;
    dst_ep = copyin ? SELF : fdset->endpoint;

    src_fds = copyin ? fdset->vir_readfds : &fdset->ready_readfds;
    dst_fds = copyin ? &fdset->readfds : fdset->vir_readfds;
    if (fdset->vir_readfds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    src_fds = copyin ? fdset->vir_writefds : &fdset->ready_writefds;
    dst_fds = copyin ? &fdset->writefds : fdset->vir_writefds;
    if (fdset->vir_writefds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    src_fds = copyin ? fdset->vir_exceptfds : &fdset->ready_exceptfds;
    dst_fds = copyin ? &fdset->exceptfds : fdset->vir_exceptfds;
    if (fdset->vir_exceptfds) {
        retval = data_copy(dst_ep, dst_fds, src_ep, src_fds, fdset_size);
        if (retval) return retval;
    }

    return 0;
}

static int fd_getops(struct select_fdset* fdset, int fd)
{
    int ops = 0;

    if (FD_ISSET(fd, &fdset->readfds)) ops |= SEL_READ;
    if (FD_ISSET(fd, &fdset->writefds)) ops |= SEL_WRITE;
    if (FD_ISSET(fd, &fdset->exceptfds)) ops |= SEL_EXCEPT;

    return ops;
}

static void fd_setfromops(struct select_fdset* fdset, int fd, int ops)
{
    if ((ops & SEL_READ) && fdset->vir_readfds &&
        FD_ISSET(fd, &fdset->readfds) && !FD_ISSET(fd, &fdset->ready_readfds)) {
        FD_SET(fd, &fdset->ready_readfds);
        fdset->nreadyfds++;
    }

    if ((ops & SEL_WRITE) && fdset->vir_writefds &&
        FD_ISSET(fd, &fdset->writefds) &&
        !FD_ISSET(fd, &fdset->ready_writefds)) {
        FD_SET(fd, &fdset->ready_writefds);
        fdset->nreadyfds++;
    }

    if ((ops & SEL_EXCEPT) && fdset->vir_exceptfds &&
        FD_ISSET(fd, &fdset->exceptfds) &&
        !FD_ISSET(fd, &fdset->ready_exceptfds)) {
        FD_SET(fd, &fdset->ready_exceptfds);
        fdset->nreadyfds++;
    }
}

static int pollwake(struct wait_queue_entry* wq_entry, void* arg)
{
    __poll_t ops = (__poll_t)(unsigned long)arg;
    struct poll_table_entry* entry =
        list_entry(wq_entry, struct poll_table_entry, wait);
    struct poll_wqueues* pwq = wq_entry->private;

    if (ops && !(ops & entry->mask)) {
        return 0;
    }

    if (pwq->polling_worker->blocked_on != WT_BLOCKED_ON_POLL) return 0;

    pwq->triggered = TRUE;

    worker_wake(pwq->polling_worker);

    return 1;
}

static void __pollwait(struct file_desc* filp, struct wait_queue_head* wq,
                       struct poll_table* wait)
{
    struct poll_wqueues* pwd = list_entry(wait, struct poll_wqueues, pt);
    struct poll_table_entry* entry = poll_get_entry(pwd);

    if (!entry) {
        return;
    }

    entry->filp = filp;
    entry->mask = wait->mask;
    entry->wq = wq;
    init_waitqueue_entry_func(&entry->wait, pollwake);
    entry->wait.private = pwd;
    waitqueue_add(wq, &entry->wait);
}

int do_poll(void)
{
    int src = self->msg_in.source;
    int retval = 0;
    int nfds = self->msg_in.u.m_vfs_poll.nfds;
    struct poll_wqueues pwq;
    struct poll_table* wait = &pwq.pt;
    int timeout_msecs = self->msg_in.u.m_vfs_poll.timeout_msecs;
    struct timer_list timer;
    struct select_timer_cb_data timer_cb;
    int timed_out = 0;
    struct pollfd* ufds = self->msg_in.u.m_vfs_poll.fds;
    struct pollfd* fds = NULL;
    size_t count = 0;

    if (nfds < 0 || nfds > OPEN_MAX) return -EINVAL;

    fds = calloc(nfds, sizeof(*fds));
    if (!fds) {
        return -ENOMEM;
    }

    retval = data_copy(SELF, fds, src, ufds, sizeof(*fds) * nfds);
    if (retval) {
        retval = -retval;
        goto free_fds;
    }

    poll_initwait(&pwq);

    if (timeout_msecs == 0) {
        wait->qproc = NULL;
        timed_out = TRUE;
    }

    for (;;) {
        int i;

        // poll each fd
        for (i = 0; i < nfds; i++) {
            struct file_desc* filp;
            int fd = fds[i].fd;
            __poll_t filter =
                demangle_poll(fds[i].events) | EPOLLERR | EPOLLHUP;
            __poll_t mask = 0;

            if (fd < 0) {
                fds[i].revents = mangle_poll(mask);
                continue;
            }

            mask = EPOLLNVAL;

            filp = get_filp(fproc, fd, RWL_WRITE);
            if (!filp) {
                fds[i].revents = mangle_poll(mask);
                continue;
            }

            wait->mask = filter;
            mask = vfs_poll(
                filp, filter | (timed_out ? 0 : POLL_NOTIFY) | POLL_ONESHOT,
                wait, fproc);

            mask &= filter;
            fds[i].revents = mangle_poll(mask);

            unlock_filp(filp);

            if (mask) {
                count++;
                wait->qproc = NULL;
            }
        }

        wait->qproc = NULL;

        // got something
        if (count || timed_out) {
            break;
        }

        // prepare to sleep
        if (!pwq.triggered) {
            if (timeout_msecs > 0) {
                timer_cb.expired = FALSE;
                timer_cb.worker = self;

                clock_t ticks;
                ticks = (timeout_msecs * system_hz + 1000L - 1) / 1000L;
                set_timer(&timer, ticks, select_timeout_check, &timer_cb);
            }

            worker_wait(WT_BLOCKED_ON_POLL);

            if (timeout_msecs > 0) {
                timed_out = timer_cb.expired;
                if (!timed_out) {
                    cancel_timer(&timer);
                }
            }

            pwq.triggered = FALSE;
        }
    }

    poll_freewait(&pwq);

    retval = data_copy(src, ufds, SELF, fds, sizeof(*fds) * nfds);
    if (retval) {
        retval = -retval;
        goto free_fds;
    }

    retval = count;

free_fds:
    free(fds);
    return retval;
}
