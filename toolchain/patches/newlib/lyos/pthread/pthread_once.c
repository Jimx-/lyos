#include <errno.h>
#include <pthread.h>
#include <sys/futex.h>
#include <limits.h>

#include "pthread_internal.h"

#define ONCE_COMPLETED 1
#define ONCE_LOCKED    2

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    unsigned int old_state =
        __atomic_load_n((unsigned int*)once_control, __ATOMIC_ACQUIRE);

    while (!(old_state & ONCE_COMPLETED)) {
        if (!(old_state & ONCE_LOCKED)) {
            if (!__atomic_compare_exchange_n(
                    (unsigned int*)once_control, &old_state, ONCE_LOCKED, 0,
                    __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))
                continue;

            init_routine();

            __atomic_exchange_n((unsigned int*)once_control, ONCE_COMPLETED,
                                __ATOMIC_RELEASE);
            futex((unsigned int*)once_control, FUTEX_WAKE, INT_MAX, NULL, NULL,
                  0);
            return 0;
        } else {
            futex((unsigned int*)once_control, FUTEX_WAIT, ONCE_LOCKED, NULL,
                  NULL, 0);
            old_state =
                __atomic_load_n((unsigned int*)once_control, __ATOMIC_ACQUIRE);
        }
    }

    return 0;
}
