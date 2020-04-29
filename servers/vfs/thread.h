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
#define mutex_t coro_mutex_t
#define cond_t coro_cond_t
#define rwlock_t coro_rwlock_t

#define mutex_init coro_mutex_init
#define mutex_trylock coro_mutex_trylock
#define mutex_lock coro_mutex_lock
#define mutex_unlock coro_mutex_unlock

#define cond_init coro_cond_init
#define cond_wait coro_cond_wait
#define cond_signal coro_cond_signal

#define rwlock_init coro_rwlock_init
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
    int err_code;

    struct worker_thread* next;

    MESSAGE msg_in;
    MESSAGE msg_out;
    MESSAGE* msg_recv;
    MESSAGE* msg_driver;
    endpoint_t recv_from;
};

#endif
