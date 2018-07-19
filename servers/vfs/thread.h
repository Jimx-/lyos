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

#include <lyos/priv.h>
#include <lyos/config.h>
#include "libpthread/pthread.h"

/* 256k stack */
#define DEFAULT_THREAD_STACK_SIZE   (1024 * 256)

#define NR_WORKER_THREADS           CONFIG_SMP_MAX_CPUS

struct worker_thread {
    int id;
    endpoint_t endpoint;
    struct priv priv;

    pthread_mutex_t event_mutex;
    pthread_cond_t event;

    struct list_head list;

    MESSAGE* driver_msg;
};

struct vfs_message {
    struct list_head list;
    MESSAGE msg;
};

PUBLIC void worker_wait();
PUBLIC void worker_wake(struct worker_thread* thread);
PUBLIC struct worker_thread* worker_self();

#endif
