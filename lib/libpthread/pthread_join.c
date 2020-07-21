#include <unistd.h>
#include <errno.h>
#include <sys/futex.h>
#include <pthread.h>

#include "pthread_internal.h"

void pthread_exit(void* value_ptr)
{
    pthread_internal_t* self = thread_self();
    int old_state = THREAD_NOT_JOINED, state;

    self->retval = value_ptr;

    while (old_state == THREAD_NOT_JOINED) {
        state = __sync_val_compare_and_swap(&self->join_state, old_state,
                                            THREAD_EXITED_NOT_JOINED);
        if (state == old_state) break;
        old_state = state;
    }

    _exit(0);
}

int pthread_join(pthread_t thread, void** retval)
{
    pthread_internal_t* join_thread = thread_handle(thread);
    pid_t pid;
    volatile pid_t* pidp;
    int old_state = THREAD_NOT_JOINED, state;

    while (old_state == THREAD_NOT_JOINED ||
           old_state == THREAD_EXITED_NOT_JOINED) {
        state = __sync_val_compare_and_swap(&join_thread->join_state, old_state,
                                            THREAD_JOINED);

        if (state == old_state) break;
        old_state = state;
    }

    if (old_state == THREAD_DETACHED || old_state == THREAD_JOINED) {
        return EINVAL;
    }

    pid = join_thread->pid;
    pidp = &join_thread->pid;

    /* wait for kernel to signal us when the thread exits */
    while (*pidp) {
        futex((int*)pidp, FUTEX_WAIT, pid, NULL, NULL, 0);
    }

    if (retval) *retval = join_thread->retval;

    return 0;
}

int pthread_detach(pthread_t thread)
{
    pthread_internal_t* tcb = thread_handle(thread);
    int old_state = THREAD_NOT_JOINED, state;

    if (!tcb) {
        return ESRCH;
    }

    while (old_state == THREAD_NOT_JOINED) {
        state = __sync_val_compare_and_swap(&tcb->join_state, old_state,
                                            THREAD_DETACHED);
        if (state == old_state) break;
        old_state = state;
    }

    if (old_state == THREAD_NOT_JOINED) {
        return 0;
    } else if (old_state == THREAD_EXITED_NOT_JOINED) {
        pthread_join(thread, NULL);
    }

    return EINVAL;
}
