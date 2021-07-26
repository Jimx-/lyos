#include <errno.h>
#include <libcoro/libcoro.h>

#include "coro_internal.h"
#include "global.h"
#include "proto.h"

int coro_rwlock_init(coro_rwlock_t* rwlock)
{
    int retval;

    if (rwlock == NULL) {
        return EINVAL;
    }

    rwlock->writer = NO_THREAD;
    rwlock->readers = 0;

    retval = coro_mutex_init(&rwlock->reader_queue, NULL);
    if (retval) {
        return retval;
    }

    retval = coro_cond_init(&rwlock->writer_queue, NULL);
    if (retval) {
        return retval;
    }

    retval = coro_mutex_init(&rwlock->mutex, NULL);
    return retval;
}

int coro_rwlock_rdlock(coro_rwlock_t* rwlock)
{
    int retval;

    if (rwlock == NULL) {
        return EINVAL;
    }

    if ((retval = coro_mutex_lock(&rwlock->reader_queue)) != 0) {
        return retval;
    }

    if ((retval = coro_mutex_unlock(&rwlock->reader_queue)) != 0) {
        return retval;
    }

    rwlock->readers++;

    return 0;
}

int coro_rwlock_wrlock(coro_rwlock_t* rwlock)
{
    int retval;

    if (rwlock == NULL) {
        return EINVAL;
    }

    if ((retval = coro_mutex_lock(&rwlock->reader_queue)) != 0) {
        return retval;
    }

    rwlock->writer = current_thread;

    if (rwlock->readers > 0) {
        if ((retval = coro_mutex_lock(&rwlock->mutex)) != 0) {
            return retval;
        }

        if ((retval = coro_cond_wait(&rwlock->writer_queue, &rwlock->mutex)) !=
            0) {
            coro_mutex_unlock(&rwlock->mutex);
            return retval;
        }

        if ((retval = coro_mutex_unlock(&rwlock->mutex)) != 0) {
            return retval;
        }
    }

    return 0;
}

int coro_rwlock_unlock(coro_rwlock_t* rwlock)
{
    int retval = 0;

    if (rwlock == NULL) {
        return EINVAL;
    }

    if (rwlock->writer == current_thread) {
        rwlock->writer = NO_THREAD;

        retval = coro_mutex_unlock(&rwlock->reader_queue);
    } else if (rwlock->readers == 0 && rwlock->writer != NO_THREAD &&
               rwlock->writer != current_thread) {
        return EPERM;
    } else {
        if (rwlock->readers == 0) {
            return EPERM;
        }

        rwlock->readers--;

        if (rwlock->readers == 0 && rwlock->writer != NO_THREAD) {
            if ((retval = coro_mutex_lock(&rwlock->mutex)) != 0) {
                return retval;
            }

            if ((retval = coro_cond_signal(&rwlock->writer_queue)) != 0) {
                coro_mutex_unlock(&rwlock->mutex);
                return retval;
            }

            retval = coro_mutex_unlock(&rwlock->mutex);
        }
    }

    return retval;
}
