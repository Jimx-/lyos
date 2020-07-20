#ifndef _PTHREAD_INTERNAL_H_
#define _PTHREAD_INTERNAL_H_

#include <sys/types.h>

#define PTHREAD_THREADS_MAX 1024

#define PTHREAD_STACK_SIZE_DEFAULT (2 * 1024 * 1024)
#define PTHREAD_GUARD_SIZE_DEFAULT (0x1000)

typedef struct pthread_internal {
    pthread_t tid;
    pid_t pid;
    char* guard_addr;
    size_t guard_size;
    int signal;

    void* retval;
    int detached;
    int terminated;
    struct pthread_internal* joining;

    void* (*start_routine)(void*);
    void* start_arg;
} pthread_internal_t;

#define CURRENT_SP  \
    ({              \
        char __csp; \
        &__csp;     \
    })

extern pthread_internal_t __pthread_initial_thread;
extern char* __pthread_initial_thread_bos;

extern pthread_internal_t* __thread_handles[PTHREAD_THREADS_MAX];

static inline pthread_internal_t* thread_self(void)
{
    char* sp = CURRENT_SP;

    if (sp >= __pthread_initial_thread_bos) {
        return &__pthread_initial_thread;
    } else {
        return (pthread_internal_t*)(((uintptr_t)sp |
                                      (PTHREAD_STACK_SIZE_DEFAULT - 1)) +
                                     1) -
               1;
    }
}

static inline pthread_internal_t* thread_handle(pthread_t thread)
{
    return __thread_handles[thread % PTHREAD_THREADS_MAX];
}

void __pthread_suspend(pthread_internal_t* self);
void __pthread_restart(pthread_internal_t* self);

#endif
