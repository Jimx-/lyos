#ifndef _PTHREAD_INTERNAL_H_
#define _PTHREAD_INTERNAL_H_

#include <sys/types.h>
#include <sys/tls.h>

#define PTHREAD_STACK_SIZE_DEFAULT (2 * 1024 * 1024)
#define PTHREAD_GUARD_SIZE_DEFAULT (0x1000)

typedef struct pthread_internal {
    pid_t tid;
    char* guard_addr;
    size_t guard_size;
    size_t mmap_size;

    void* retval;
    int join_state;

    void* (*start_routine)(void*);
    void* start_arg;
} pthread_internal_t;

enum thread_join_state {
    THREAD_NOT_JOINED,
    THREAD_EXITED_NOT_JOINED,
    THREAD_DETACHED,
    THREAD_JOINED,
};

extern pthread_internal_t __pthread_initial_thread;

static inline pthread_internal_t* thread_handle(pthread_t thread)
{
    struct tls_tcb* tcb = (struct tls_tcb*)thread;

    if (!tcb->tcb_pthread) return &__pthread_initial_thread;

    return (pthread_internal_t*)tcb->tcb_pthread;
}

static inline pthread_internal_t* thread_self(void)
{
    return thread_handle((pthread_t)__libc_get_tls_tcb());
}

#endif
