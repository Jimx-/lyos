#include <errno.h>
#include <libcoro/libcoro.h>

#include "coro_internal.h"
#include "global.h"
#include "proto.h"

int coro_cond_init(coro_cond_t* cond, coro_condattr_t* attr)
{
    if (cond == NULL) {
        return EINVAL;
    }

    coro_queue_init(&cond->wait_queue);
    return 0;
}

int coro_cond_signal(coro_cond_t* cond)
{
    coro_thread_t thread;

    if (cond == NULL) {
        return EINVAL;
    }

    thread = coro_queue_dequeue(&cond->wait_queue);
    if (thread != NO_THREAD) coro_unsuspend(thread);

    return 0;
}

int coro_cond_broadcast(coro_cond_t* cond)
{
    coro_thread_t thread;

    if (cond == NULL) {
        return EINVAL;
    }

    while (!coro_queue_empty(&cond->wait_queue)) {
        thread = coro_queue_dequeue(&cond->wait_queue);
        if (thread != NO_THREAD) coro_unsuspend(thread);
    }

    return 0;
}

int coro_cond_wait(coro_cond_t* cond, coro_mutex_t* mutex)
{
    int retval;

    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }

    if ((retval = coro_mutex_unlock(mutex)) != 0) {
        return retval;
    }

    coro_suspend(CR_BLOCKED);

    if ((retval = coro_mutex_lock(mutex)) != 0) {
        return retval;
    }

    return 0;
}
