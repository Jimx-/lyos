#include <pthread.h>

#include "pthread_internal.h"

int pthread_barrier_init(pthread_barrier_t* restrict barrier,
                         const pthread_barrierattr_t* restrict attr,
                         unsigned count)
{
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) { return 0; }

int pthread_barrier_destroy(pthread_barrier_t* barrier) { return 0; }
