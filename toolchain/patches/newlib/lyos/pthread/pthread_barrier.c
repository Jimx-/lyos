#include <pthread.h>

#include "pthread_internal.h"

#include <errno.h>
#include <limits.h>
#include <sys/futex.h>

#define BARRIER_GENERATION (1U << 31)
#define BARRIER_WAITERS_MASK (BARRIER_GENERATION - 1)

int pthread_barrier_init(pthread_barrier_t* restrict barrier,
                         const pthread_barrierattr_t* restrict attr,
                         unsigned count)
{
    if (!barrier || count == 0 || count > BARRIER_WAITERS_MASK) return EINVAL;
    (void)attr;

    barrier->waiting = 0;
    barrier->count = count;
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier)
{
    if (!barrier || barrier->count == 0) return EINVAL;

    unsigned int old =
        __atomic_fetch_add(&barrier->waiting, 1, __ATOMIC_ACQ_REL);
    unsigned int generation = old & BARRIER_GENERATION;
    unsigned int arrived = (old & BARRIER_WAITERS_MASK) + 1;

    if (arrived == barrier->count) {
        __atomic_store_n(&barrier->waiting, generation ^ BARRIER_GENERATION,
                         __ATOMIC_RELEASE);
        futex((int*)&barrier->waiting, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }

    for (;;) {
        unsigned int state =
            __atomic_load_n(&barrier->waiting, __ATOMIC_ACQUIRE);
        if ((state & BARRIER_GENERATION) != generation) return 0;
        futex((int*)&barrier->waiting, FUTEX_WAIT, state, NULL, NULL, 0);
    }
}

int pthread_barrier_destroy(pthread_barrier_t* barrier)
{
    if (!barrier || barrier->count == 0) return EINVAL;
    if (__atomic_load_n(&barrier->waiting, __ATOMIC_RELAXED) &
        BARRIER_WAITERS_MASK)
        return EBUSY;

    barrier->count = 0;
    return 0;
}
