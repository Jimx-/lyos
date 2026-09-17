#include <pthread.h>

#include "pthread_internal.h"

#include <string.h>
#include <errno.h>
#include <sys/futex.h>

typedef struct {
    unsigned int state;
    unsigned int owner;
} pthread_mutex_internal_t;

#define MUTEXATTR_TYPE_MASK     0x000f
#define MUTEXATTR_SHARED_MASK   0x0010
#define MUTEXATTR_PROTOCOL_MASK 0x0020

#define MUTEX_TYPE_MASK       (3 << 14)
#define MUTEX_TYPE_NORMAL     (0 << 14)
#define MUTEX_TYPE_RECURSIVE  (PTHREAD_MUTEX_RECURSIVE << 14)
#define MUTEX_TYPE_ERRORCHECK (PTHREAD_MUTEX_ERRORCHECK << 14)

/* Bits 2..13 count recursive acquisitions beyond the first. */
#define MUTEX_COUNT_ONE  (1U << 2)
#define MUTEX_COUNT_MASK ((1U << 14) - MUTEX_COUNT_ONE)

#define MUTEX_STATE_MASK               3
#define MUTEX_STATE_UNLOCKED           0
#define MUTEX_STATE_LOCKED_UNCONTENDED 1
#define MUTEX_STATE_LOCKED_CONTENDED   2

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
    *attr = PTHREAD_MUTEX_NORMAL;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
    *attr = -1;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type_p)
{
    int type = (*attr & MUTEXATTR_TYPE_MASK);

    if (type < PTHREAD_MUTEX_NORMAL || type > PTHREAD_MUTEX_ERRORCHECK) {
        return EINVAL;
    }

    *type_p = type;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type)
{
    if (type < PTHREAD_MUTEX_NORMAL || type > PTHREAD_MUTEX_ERRORCHECK) {
        return EINVAL;
    }

    *attr = (*attr & ~MUTEXATTR_TYPE_MASK) | type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t* pmutex, const pthread_mutexattr_t* attr)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;
    int type = PTHREAD_MUTEX_NORMAL;
    if (attr) {
        int ret = pthread_mutexattr_gettype(attr, &type);
        if (ret) return ret;
    }
    memset(mutex, 0, sizeof(pthread_mutex_internal_t));
    mutex->state = (unsigned int)type << 14;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* pmutex)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;

    unsigned int unlocked =
        __atomic_load_n(&mutex->state, __ATOMIC_RELAXED) & MUTEX_TYPE_MASK;
    if (__sync_bool_compare_and_swap(&mutex->state, unlocked, 0xffffffff)) {
        return 0;
    }

    return EBUSY;
}

static inline int
__pthread_normal_mutex_trylock(pthread_mutex_internal_t* mutex)
{
    unsigned int unlocked =
        __atomic_load_n(&mutex->state, __ATOMIC_RELAXED) & MUTEX_TYPE_MASK;
    unsigned int locked_uncontended = unlocked | MUTEX_STATE_LOCKED_UNCONTENDED;

    unsigned int old_state = unlocked;
    if (__sync_bool_compare_and_swap(&mutex->state, old_state,
                                     locked_uncontended)) {
        return 0;
    }
    return EBUSY;
}

static inline int __pthread_normal_mutex_lock(pthread_mutex_internal_t* mutex,
                                              const struct timespec* abs_time)
{
    if (__pthread_normal_mutex_trylock(mutex) == 0) return 0;

    unsigned int state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    for (;;) {
        /* Preserve the owner's recursion count when marking contention. */
        unsigned int desired =
            (state & ~MUTEX_STATE_MASK) | MUTEX_STATE_LOCKED_CONTENDED;
        if (!__atomic_compare_exchange_n(&mutex->state, &state, desired, 1,
                                         __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            continue;
        if (!(state & MUTEX_STATE_MASK)) return 0;
        futex((int*)&mutex->state, FUTEX_WAIT, desired, abs_time, NULL, 0);
        state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    }

    return 0;
}

static inline void
__pthread_normal_mutex_unlock(pthread_mutex_internal_t* mutex)
{
    unsigned int unlocked =
        __atomic_load_n(&mutex->state, __ATOMIC_RELAXED) & MUTEX_TYPE_MASK;
    unsigned int locked_contended = MUTEX_STATE_LOCKED_CONTENDED;

    if ((__atomic_exchange_n(&mutex->state, unlocked, __ATOMIC_RELEASE) &
         MUTEX_STATE_MASK) == locked_contended) {
        futex((int*)&mutex->state, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}

static int __pthread_mutex_relock(pthread_mutex_internal_t* mutex)
{
    unsigned int state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    if ((state & MUTEX_TYPE_MASK) == MUTEX_TYPE_ERRORCHECK) return EDEADLK;
    if ((state & MUTEX_COUNT_MASK) == MUTEX_COUNT_MASK) return EAGAIN;
    __atomic_fetch_add(&mutex->state, MUTEX_COUNT_ONE, __ATOMIC_RELAXED);
    return 0;
}

static int __pthread_mutex_lock_timeout(pthread_mutex_internal_t* mutex,
                                        const struct timespec* abs_time)
{
    unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    unsigned int type = old_state & MUTEX_TYPE_MASK;

    if (type == MUTEX_TYPE_NORMAL) {
        return __pthread_normal_mutex_lock(mutex, abs_time);
    }
    if (type != MUTEX_TYPE_RECURSIVE && type != MUTEX_TYPE_ERRORCHECK)
        return EINVAL;
    unsigned int self = (unsigned int)thread_self()->tid;
    if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == self)
        return __pthread_mutex_relock(mutex);
    int ret = __pthread_normal_mutex_lock(mutex, abs_time);
    if (!ret) __atomic_store_n(&mutex->owner, self, __ATOMIC_RELAXED);
    return ret;
}

int pthread_mutex_lock(pthread_mutex_t* pmutex)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;

    unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    unsigned int type = old_state & MUTEX_TYPE_MASK;

    if (type == MUTEX_TYPE_NORMAL) {
        if (__pthread_normal_mutex_trylock(mutex) == 0) return 0;
    }

    return __pthread_mutex_lock_timeout(mutex, NULL);
}

int pthread_mutex_timedlock(pthread_mutex_t* pmutex,
                            const struct timespec* abs_time)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;

    unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    unsigned int type = old_state & MUTEX_TYPE_MASK;

    if (type == MUTEX_TYPE_NORMAL) {
        if (__pthread_normal_mutex_trylock(mutex) == 0) return 0;
    }

    return __pthread_mutex_lock_timeout(mutex, abs_time);
}

int pthread_mutex_unlock(pthread_mutex_t* pmutex)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;

    unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    unsigned int type = old_state & MUTEX_TYPE_MASK;

    if (type == MUTEX_TYPE_NORMAL) {
        __pthread_normal_mutex_unlock(mutex);
        return 0;
    }
    if (type != MUTEX_TYPE_RECURSIVE && type != MUTEX_TYPE_ERRORCHECK)
        return EINVAL;
    if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) !=
        (unsigned int)thread_self()->tid)
        return EPERM;
    if (old_state & MUTEX_COUNT_MASK) {
        __atomic_fetch_sub(&mutex->state, MUTEX_COUNT_ONE, __ATOMIC_RELAXED);
        return 0;
    }
    __atomic_store_n(&mutex->owner, 0, __ATOMIC_RELAXED);
    __pthread_normal_mutex_unlock(mutex);
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* pmutex)
{
    pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*)pmutex;

    unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
    unsigned int type = old_state & MUTEX_TYPE_MASK;

    if (type == MUTEX_TYPE_NORMAL) {
        return __pthread_normal_mutex_trylock(mutex);
    }
    if (type != MUTEX_TYPE_RECURSIVE && type != MUTEX_TYPE_ERRORCHECK)
        return EINVAL;
    unsigned int self = (unsigned int)thread_self()->tid;
    if (__atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == self) {
        if (type == MUTEX_TYPE_ERRORCHECK) return EBUSY;
        return __pthread_mutex_relock(mutex);
    }
    int ret = __pthread_normal_mutex_trylock(mutex);
    if (!ret) __atomic_store_n(&mutex->owner, self, __ATOMIC_RELAXED);
    return ret;
}
