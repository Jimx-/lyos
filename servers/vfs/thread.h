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

#ifndef _VFS_THREAD_H_
#define _VFS_THREAD_H_

#include <libcoro/libcoro.h>

#define thread_t coro_thread_t
#define mutex_t  coro_mutex_t
#define cond_t   coro_cond_t
#define rwlock_t coro_rwlock_t

#define mutex_init    coro_mutex_init
#define mutex_trylock coro_mutex_trylock
#define mutex_lock    coro_mutex_lock
#define mutex_unlock  coro_mutex_unlock

#define cond_init   coro_cond_init
#define cond_wait   coro_cond_wait
#define cond_signal coro_cond_signal

#define rwlock_init   coro_rwlock_init
#define rwlock_rdlock coro_rwlock_rdlock
#define rwlock_wrlock coro_rwlock_wrlock
#define rwlock_unlock coro_rwlock_unlock

typedef enum {
    RWL_NONE,
    RWL_READ,
    RWL_WRITE,
} rwlock_type_t;

struct fproc;

struct worker_thread {
    thread_t tid;
    struct fproc* fproc;
    mutex_t event_mutex;
    cond_t event;
    int blocked_on;
    int err_code;

    struct worker_thread* next;

    MESSAGE msg_in;
    MESSAGE msg_out;
    MESSAGE* msg_recv;
    MESSAGE* msg_driver;
    endpoint_t recv_from;
};

/* In certain cases, a worker thread could be put in more than one wait queue
 * before it is blocked. e.g. a thread waiting for the polling reply from the
 * driver while it is still in the device's polling queue.  In that case, the
 * thread can be accidentally unblocked by a polling notification from the
 * driver (instead of the polling reply message it is waiting for).  To prevent
 * this, we use the ~blocked_on~ flag to record the reason why a thread is
 * blocked so that it can only be unblocked by the correct signal */
#define WT_BLOCKED_ON_NONE    0
#define WT_BLOCKED_ON_LOCK    1
#define WT_BLOCKED_ON_DRV_MSG 2
#define WT_BLOCKED_ON_POLL    3
#define WT_BLOCKED_ON_EPOLL   4
#define WT_BLOCKED_ON_PIPE    5
#define WT_BLOCKED_ON_EFD     6
#define WT_BLOCKED_ON_SFD     7
#define WT_BLOCKED_ON_TFD     8
#define WT_BLOCKED_ON_FLOCK   9
#define WT_BLOCKED_ON_INOTIFY 10

#endif
