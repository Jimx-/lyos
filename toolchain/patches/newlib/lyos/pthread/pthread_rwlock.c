#include <pthread.h>

#include "pthread_internal.h"

#include <string.h>
#include <errno.h>
#include <sys/futex.h>

#define RWS_PD_WRITERS (1 << 0)
#define RWS_PD_READERS (1 << 1)
#define RCNT_SHIFT     2
#define RCNT_INC_STEP  (1 << RCNT_SHIFT)
#define RWS_WRLOCKED   (1 << 31)

#define RW_RDLOCKED(l) ((l) >= RCNT_INC_STEP)
#define RW_WRLOCKED(l) ((l)&RWS_WRLOCKED)

int pthread_rwlock_init(pthread_rwlock_t* lock,
                        const pthread_rwlockattr_t* attr)
{
    memset(lock, 0, sizeof(pthread_rwlock_t));
    pthread_mutex_init(&lock->pending_mutex, NULL);

    lock->pending_reader_serial = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* lock)
{
    int state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

    if (RW_RDLOCKED(state) || RW_WRLOCKED(state)) {
        return EBUSY;
    }

    return 0;
}

static int __can_rdlock(int state) { return !RW_WRLOCKED(state); }

static int __pthread_rwlock_tryrdlock(pthread_rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

    while (__can_rdlock(old_state)) {
        int new_state = old_state + RCNT_INC_STEP;
        if (!RW_RDLOCKED(new_state)) return EAGAIN;

        if (__atomic_compare_exchange_n(&lock->state, &old_state, new_state, 1,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return 0;
    }
    return EBUSY;
}

static int __pthread_rwlock_timedrdlock(pthread_rwlock_t* lock,
                                        const struct timespec* abs_timeout)
{
    /*
    if (__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) == pthread_self()) {
        return EDEADLK;
    }*/

    while (1) {
        int ret = __pthread_rwlock_tryrdlock(lock);
        if (ret == 0 || ret == EAGAIN) return ret;

        int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if (__can_rdlock(old_state)) continue;

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_reader_count++;

        old_state =
            __atomic_fetch_or(&lock->state, RWS_PD_READERS, __ATOMIC_RELAXED);
        int old_serial = lock->pending_reader_serial;
        pthread_mutex_unlock(&lock->pending_mutex);

        if (!__can_rdlock(old_state)) {
            ret = futex((int*)&lock->pending_reader_serial, FUTEX_WAIT,
                        old_serial, abs_timeout, NULL, 0);
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

int pthread_rwlock_rdlock(pthread_rwlock_t* lock)
{
    if (__pthread_rwlock_tryrdlock(lock) == 0) {
        return 0;
    }

    return __pthread_rwlock_timedrdlock(lock, NULL);
}

static int __can_wrlock(int state)
{
    return !(RW_WRLOCKED(state) && RW_RDLOCKED(state));
}

static int __pthread_rwlock_trywrlock(pthread_rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

    while (__can_wrlock(old_state)) {
        if (__atomic_compare_exchange_n(&lock->state, &old_state,
                                        old_state | RWS_WRLOCKED, 1,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            //__atomic_store_n(&lock->owner, pthread_self(), __ATOMIC_RELAXED);
            return 0;
        }
    }
    return EBUSY;
}

static int __pthread_rwlock_timedwrlock(pthread_rwlock_t* lock,
                                        const struct timespec* abs_timeout)
{
    /*
    if (__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) == pthread_self()) {
        return EDEADLK;
    }*/

    while (1) {
        int ret = __pthread_rwlock_trywrlock(lock);
        if (ret == 0) return ret;

        int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
        if (__can_wrlock(old_state)) continue;

        pthread_mutex_lock(&lock->pending_mutex);
        lock->pending_writer_count++;

        old_state =
            __atomic_fetch_or(&lock->state, RWS_PD_WRITERS, __ATOMIC_RELAXED);
        int old_serial = lock->pending_writer_serial;
        pthread_mutex_unlock(&lock->pending_mutex);

        if (!__can_wrlock(old_state)) {
            ret = futex((int*)&lock->pending_writer_serial, FUTEX_WAIT,
                        old_serial, abs_timeout, NULL, 0);
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

int pthread_rwlock_wrlock(pthread_rwlock_t* lock)
{
    if (__pthread_rwlock_trywrlock(lock) == 0) return 0;

    return __pthread_rwlock_timedwrlock(lock, NULL);
}

int pthread_rwlock_unlock(pthread_rwlock_t* lock)
{
    int old_state = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);
    if (RW_WRLOCKED(old_state)) {
        /* if (pthread_self() != __atomic_load_n(&lock->owner,
         * __ATOMIC_RELAXED)) return EPERM; */
        __atomic_store_n(&lock->owner, 0, __ATOMIC_RELAXED);
        old_state =
            __atomic_fetch_and(&lock->state, ~RWS_WRLOCKED, __ATOMIC_RELEASE);

        if (!(old_state & RWS_PD_WRITERS) && !(old_state & RWS_PD_READERS))
            return 0;
    } else if (RW_RDLOCKED(old_state)) {
        old_state =
            __atomic_fetch_sub(&lock->state, RCNT_INC_STEP, __ATOMIC_RELEASE);

        if ((old_state >> RCNT_SHIFT) != 1 ||
            !(old_state & RWS_PD_WRITERS && old_state & RWS_PD_READERS))
            return 0;
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

        futex((int*)&lock->pending_reader_serial, FUTEX_WAKE, 0x7fffffff, NULL,
              NULL, 0);
    } else {
        pthread_mutex_lock(&lock->pending_mutex);
    }

    return 0;
}
