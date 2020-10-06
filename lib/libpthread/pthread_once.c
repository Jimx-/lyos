#include <errno.h>
#include <pthread.h>

#include "pthread_internal.h"

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    init_routine();
    return 0;
}
