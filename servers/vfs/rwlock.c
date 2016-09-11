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
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/ipc.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sys/futex.h>

#include "rwlock.h"

#define RWS_PD_WRITERS  (1 << 0)
#define RWS_PD_READERS  (1 << 1)
#define RCNT_SHIFT      2
#define RCNT_INC_STEP   (1 << RCNT_SHIFT)
#define RWS_WRLOCKED    (1 << 31)

#define RW_RDLOCKED(l)  ((l) >= RCNT_INC_STEP)
#define RW_WRLOCKED(l)  ((l) & RWS_WRLOCKED)

void rwlock_init(rwlock_t* lock)
{
    memset(lock, 0, sizeof(rwlock_t));
    pthread_mutex_init(&lock->pending_mutex, NULL);

    lock->pending_reader_serial = 0;
}

PRIVATE int __can_rdlock(int state)
{
    return !RW_WRLOCKED(state);
}

PRIVATE int __rwlock_tryrdlock(rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

    while (__can_rdlock(old_state)) {
        int new_state = old_state + RCNT_INC_STEP;
        if (!RW_RDLOCKED(new_state)) return EAGAIN;

        if (__atomic_compare_exchange_n(&lock->state, &old_state, new_state, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return 0;
    }
    return EBUSY;
}

PRIVATE int __rwlock_timedrdlock(rwlock_t* lock, const struct timespec* abs_timeout)
{
    if (__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) == worker_self()->id + 1) {
        return EDEADLK;
    }

    while (1) {
        int ret = __rwlock_tryrdlock(lock);
        if (ret == 0 || ret == EAGAIN) return ret;

        int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if (__can_rdlock(old_state)) continue;

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_reader_count++;
        
        old_state = __atomic_fetch_or(&lock->state, RWS_PD_READERS, __ATOMIC_RELAXED);
        int old_serial = lock->pending_reader_serial;
        pthread_mutex_unlock(&lock->pending_mutex);

        if (!__can_rdlock(old_state)) {
            ret = futex((int*)&lock->pending_reader_serial, FUTEX_WAIT, old_serial, abs_timeout, NULL, 0);
        }

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_reader_count--;
        if (lock->pending_reader_count == 0) {
            __atomic_fetch_and(&lock->state, ~RWS_PD_READERS, __ATOMIC_RELAXED);
        }
        pthread_mutex_unlock(&lock->pending_mutex);
        if (ret) return ret;
    }
    return 0;
}

PRIVATE int rwlock_rdlock(rwlock_t* lock)
{
    if (__rwlock_tryrdlock(lock) == 0) {
        return 0;
    } 

    return __rwlock_timedrdlock(lock, NULL);
}

PRIVATE int __can_wrlock(int state)
{
    return !(RW_WRLOCKED(state) && RW_RDLOCKED(state));
}

PRIVATE int __rwlock_trywrlock(rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

    while (__can_wrlock(old_state)) {
        if (__atomic_compare_exchange_n(&lock->state, &old_state, old_state | RWS_WRLOCKED, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            __atomic_store_n(&lock->owner, worker_self()->id + 1, __ATOMIC_RELAXED);
            return 0;
        }
    }
    return EBUSY;
}

PRIVATE int __rwlock_timedwrlock(rwlock_t* lock, const struct timespec* abs_timeout)
{
    if (__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) == worker_self()->id + 1) {
        return EDEADLK;
    }

    while (1) {
        int ret = __rwlock_trywrlock(lock);
        if (ret == 0) return ret;

        int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if (__can_wrlock(old_state)) continue;

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_writer_count++;
        
        old_state = __atomic_fetch_or(&lock->state, RWS_PD_WRITERS, __ATOMIC_RELAXED);
        int old_serial = lock->pending_writer_serial;
        pthread_mutex_unlock(&lock->pending_mutex);

        if (!__can_wrlock(old_state)) {
            ret = futex((int*)&lock->pending_writer_serial, FUTEX_WAIT, old_serial, abs_timeout, NULL, 0);
        }

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_writer_count--;
        if (lock->pending_writer_count == 0) {
            __atomic_fetch_and(&lock->state, ~RWS_PD_WRITERS, __ATOMIC_RELAXED);
        }
        pthread_mutex_unlock(&lock->pending_mutex);
        if (ret) return ret;
    }
    return 0;
}

PRIVATE int rwlock_wrlock(rwlock_t* lock)
{
    if (__rwlock_trywrlock(lock) == 0) return 0;

    return __rwlock_timedwrlock(lock, NULL);
}

PUBLIC int rwlock_lock(rwlock_t* lock, rwlock_type_t type)
{
    switch (type) {
    case RWL_NONE:
        return 0;
    case RWL_READ:
        return rwlock_rdlock(lock);
    case RWL_WRITE:
        return rwlock_wrlock(lock);
    }

    return EINVAL;
}

PUBLIC int rwlock_unlock(rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
    if (RW_WRLOCKED(old_state)) {
        if (worker_self()->id + 1 != __atomic_load_n(&lock->owner, __ATOMIC_RELAXED)) return EPERM;
        __atomic_store_n(&lock->owner, 0, __ATOMIC_RELAXED);
        old_state = __atomic_fetch_and(&lock->state, ~RWS_WRLOCKED, __ATOMIC_RELEASE);

        if (!(old_state & RWS_PD_WRITERS) && !(old_state & RWS_PD_READERS)) return 0;
    } else if (RW_RDLOCKED(old_state)) {
        old_state = __atomic_fetch_sub(&lock->state, RCNT_INC_STEP, __ATOMIC_RELEASE);
        
        if ((old_state >> RCNT_SHIFT) != 1 || !(old_state & RWS_PD_WRITERS && old_state & RWS_PD_READERS)) return 0;
    } else
        return EPERM;

    pthread_mutex_lock(&lock->pending_mutex);
    if (lock->pending_writer_count) {
        lock->pending_writer_serial++;
        pthread_mutex_unlock(&lock->pending_mutex);

        futex((int*)&lock->pending_writer_serial, FUTEX_WAKE, 1, NULL, NULL, 0);
    } else if (lock->pending_reader_count) {
        lock->pending_reader_serial++;
        pthread_mutex_unlock(&lock->pending_mutex);

        futex((int*)&lock->pending_reader_serial, FUTEX_WAKE, 0x7fffffff, NULL, NULL, 0);
    } else {
        pthread_mutex_lock(&lock->pending_mutex);
    }

    return 0;
}
