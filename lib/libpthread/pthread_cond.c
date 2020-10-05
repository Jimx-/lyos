#include <pthread.h>

#include "pthread_internal.h"

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/futex.h>

#define COND_INC_STEP 0x4

int pthread_condattr_init(pthread_condattr_t* attr) { return 0; }

int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clockid)
{
    return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr)
{
    if (!attr) {
        cond->state = 0;
    }

    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) { return 0; }

static int __pthread_cond_pulse(pthread_cond_t* cond, int count)
{
    __atomic_fetch_add(&cond->state, COND_INC_STEP, __ATOMIC_RELAXED);
    futex((int*)&cond->state, FUTEX_WAKE, count, NULL, NULL, 0);

    return 0;
}

static int __pthread_cond_timedwait(pthread_cond_t* cond,
                                    pthread_mutex_t* mutex,
                                    const struct timespec* abs_time)
{
    unsigned int old_state = __atomic_load_n(&cond->state, __ATOMIC_RELAXED);

    pthread_mutex_unlock(mutex);
    futex((int*)&cond->state, FUTEX_WAIT, old_state, abs_time, NULL, 0);
    pthread_mutex_lock(mutex);

    return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    return __pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abs_time)
{
    return __pthread_cond_timedwait(cond, mutex, abs_time);
}

int pthread_cond_signal(pthread_cond_t* cond)
{
    return __pthread_cond_pulse(cond, 1);
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
    return __pthread_cond_pulse(cond, INT_MAX);
}
