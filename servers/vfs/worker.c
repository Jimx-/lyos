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
#include <lyos/config.h>
#include <stddef.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <lyos/fs.h>

#include "types.h"
#include "global.h"
#include "proto.h"
#include "thread.h"

#define WORKER_STACKSIZE ((size_t)0x4000)

static int block_all;
static size_t pending;

static void* worker_main(void* arg);
static void worker_sleep(void);
static int worker_get_work(void);
static void worker_assign(struct fproc* fp);
static void worker_try_assign(struct fproc* fp);

int rwlock_lock(rwlock_t* rwlock, rwlock_type_t lock_type)
{
    int retval;
    struct worker_thread* old_self;

    switch (lock_type) {
    case RWL_NONE:
        break;
    case RWL_READ:
    case RWL_WRITE:
        /* save worker context as we may be blocked */
        old_self = worker_suspend(WT_BLOCKED_ON_LOCK);

        if (lock_type == RWL_READ) {
            retval = rwlock_rdlock(rwlock);
        } else {
            retval = rwlock_wrlock(rwlock);
        }

        worker_resume(old_self);
        return retval;
    default:
        return EINVAL;
    }

    return 0;
}

static inline int has_pending_tasks(void) { return pending > 0 && !block_all; }

void worker_init(void)
{
    struct worker_thread* wp;
    coro_attr_t attr;

    block_all = 0;
    pending = 0;

    coro_attr_init(&attr);
    coro_attr_setstacksize(&attr, WORKER_STACKSIZE);

    for (wp = workers; wp < workers + NR_WORKER_THREADS; wp++) {
        wp->fproc = NULL;
        wp->recv_from = NO_TASK;
        wp->next = NULL;

        if (mutex_init(&wp->event_mutex, NULL) != 0) {
            panic("failed to initialize mutex");
        }

        if (cond_init(&wp->event, NULL) != 0) {
            panic("failed to initialize condition variable");
        }

        if (coro_thread_create(&wp->tid, &attr, worker_main, wp) != 0) {
            panic("failed to start worker thread");
        }
    }

    self = NULL;
    worker_yield();
}

static void worker_sleep(void)
{
    struct worker_thread* thread = self;

    if (mutex_lock(&thread->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }

    if (cond_wait(&thread->event, &thread->event_mutex) != 0) {
        panic("failed to wait on worker event");
    }

    if (mutex_unlock(&thread->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }

    self = thread;
}

void worker_yield(void)
{
    coro_yield_all();
    self = NULL;
}

static int worker_get_work(void)
{
    struct fproc* fp;

    if (has_pending_tasks()) {
        for (fp = fproc_table; fp < fproc_table + NR_PROCS; fp++) {
            if (fp->flags & FPF_PENDING) {
                self->fproc = fp;
                fproc->worker = self;
                fp->flags &= FPF_PENDING;
                pending--;

                return TRUE;
            }
        }
    }

    worker_sleep();

    return self->fproc != NULL;
}

void worker_dispatch(struct fproc* fp, void (*func)(void), MESSAGE* msg)
{
    fp->func = func;
    fp->msg = *msg;

    worker_try_assign(fp);
}

static void worker_assign(struct fproc* fp)
{
    struct worker_thread* wp;

    for (wp = workers; wp < workers + NR_WORKER_THREADS; wp++) {
        if (wp->fproc == NULL) break;
    }

    if (wp == workers + NR_WORKER_THREADS) {
        panic("no worker for new task");
    }

    fp->worker = wp;
    wp->fproc = fp;

    worker_wake(wp);
}

static void worker_try_assign(struct fproc* fp)
{
    if (!block_all) {
        worker_assign(fp);
    } else {
        fp->flags |= FPF_PENDING;
        pending++;
    }
}

void worker_wake(struct worker_thread* worker)
{
    if (mutex_lock(&worker->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }

    if (cond_signal(&worker->event) != 0) {
        panic("failed to signal worker event");
    }

    if (mutex_unlock(&worker->event_mutex) != 0) {
        panic("failed to lock event mutex");
    }
}

struct worker_thread* worker_suspend(int why)
{
    self->err_code = err_code;
    self->blocked_on = why;

    return self;
}

void worker_resume(struct worker_thread* worker)
{
    self = worker;
    fproc = worker->fproc;
    err_code = worker->err_code;
    self->blocked_on = WT_BLOCKED_ON_NONE;
}

void worker_wait(int why)
{
    struct worker_thread* worker = worker_suspend(why);

    worker_sleep();

    worker_resume(worker);
}

void worker_allow(int allow)
{
    struct fproc* fp;

    block_all = !allow;

    if (!has_pending_tasks()) {
        return;
    }

    for (fp = fproc_table; fp < fproc_table + NR_PROCS; fp++) {
        if (fp->flags & FPF_PENDING) {
            fp->flags &= FPF_PENDING;
            pending--;
            worker_assign(fp);

            if (!has_pending_tasks()) {
                return;
            }
        }
    }
}

struct worker_thread* worker_get(thread_t tid)
{
    struct worker_thread* wp;

    for (wp = workers; wp < workers + NR_WORKER_THREADS; wp++) {
        if (wp->tid == tid) {
            return wp;
        }
    }

    return NULL;
}

void revive_proc(endpoint_t endpoint, MESSAGE* msg)
{
    send_recv(SEND_NONBLOCK, endpoint, msg);
}

static void* worker_main(void* arg)
{
    self = (struct worker_thread*)arg;

    while (worker_get_work()) {
        fproc = self->fproc;

        lock_fproc(fproc);

        if (fproc->func != NULL) {
            self->msg_in = fproc->msg;
            err_code = 0;

            fproc->func();

            fproc->func = NULL;
        }

        unlock_fproc(fproc);

        self->fproc = NULL;
        fproc->worker = NULL;
    }

    return NULL;
}
