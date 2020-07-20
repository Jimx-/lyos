#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "pthread_internal.h"

void pthread_exit(void* value_ptr)
{
    pthread_internal_t* self = thread_self();

    self->retval = value_ptr;
    self->terminated = 1;

    if (self->joining) {
        __pthread_restart(self->joining);
    }

    _exit(0);
}

int pthread_join(pthread_t thread, void** retval)
{
    pthread_internal_t* self = thread_self();
    pthread_internal_t* join_thread = thread_handle(thread);

    if (join_thread->detached || join_thread->joining != NULL) {
        return EINVAL;
    }

    if (!join_thread->terminated) {
        join_thread->joining = self;

        __pthread_suspend(self);
    }

    if (retval) *retval = join_thread->retval;

    return 0;
}
