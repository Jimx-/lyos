#include <errno.h>
#include <libcoro/libcoro.h>

#include "coro_internal.h"
#include "global.h"
#include "proto.h"

int coro_mutex_init(coro_mutex_t* mutex, coro_mutexattr_t* attr)
{
    if (mutex == NULL) {
        return EINVAL;
    }

    coro_queue_init(&mutex->wait_queue);
    mutex->owner = NO_THREAD;

    return 0;
}

int coro_mutex_trylock(coro_mutex_t* mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

    if (mutex->owner == current_thread) {
        return EDEADLK;
    } else if (mutex->owner == NO_THREAD) {
        mutex->owner = current_thread;
        return 0;
    }

    return EBUSY;
}

int coro_mutex_lock(coro_mutex_t* mutex)
{
    int retval = coro_mutex_trylock(mutex);

    if (retval == EBUSY) {
        coro_queue_enqueue(&mutex->wait_queue, current_thread);
        coro_suspend(CR_BLOCKED);

        return 0;
    }

    return retval;
}

int coro_mutex_unlock(coro_mutex_t* mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

    if (mutex->owner != current_thread) {
        return EPERM;
    }

    mutex->owner = coro_queue_dequeue(&mutex->wait_queue);
    if (mutex->owner != NO_THREAD) coro_unsuspend(mutex->owner);

    return 0;
}
