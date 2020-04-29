#ifndef _CORO_INTERNAL_H_
#define _CORO_INTERNAL_H_

#include <ucontext.h>

typedef enum {
    CR_DEAD,
    CR_BLOCKED,
    CR_RUNNABLE,
    CR_EXITING,
} coro_state_t;

struct __coro_tcb {
    coro_thread_t id;
    coro_state_t state;
    coro_attr_t attr;
    ucontext_t context;
    coro_mutex_t exitm;
    coro_cond_t exited;

    void* (*proc)(void*);
    void* arg;
    void* result;

    struct __coro_tcb* next;
};

typedef struct __coro_tcb coro_tcb_t;

#endif
