#include <pthread.h>

#include "pthread_internal.h"

const pthread_attr_t __pthread_default_attr = {
    schedparam : {sched_priority : 0},
    stacksize : PTHREAD_STACK_SIZE_DEFAULT - PTHREAD_GUARD_SIZE_DEFAULT,
    stackaddr : NULL,
    guardsize : PTHREAD_GUARD_SIZE_DEFAULT,
    detachstate : PTHREAD_CREATE_JOINABLE,
    inheritsched : PTHREAD_EXPLICIT_SCHED,
    scope : PTHREAD_SCOPE_SYSTEM,
    schedpolicy : SCHED_OTHER
};

int pthread_attr_init(pthread_attr_t* attr)
{
    *attr = __pthread_default_attr;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate)
{
    *detachstate = attr->detachstate;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) { return 0; }
