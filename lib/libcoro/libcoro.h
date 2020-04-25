#ifndef _LIBCORO_H_
#define _LIBCORO_H_

#include <sys/types.h>

typedef int coro_thread_t;

struct __coro_tcb;

typedef struct {
    size_t stacksize;
    void* stackaddr;
} coro_thread_attr_t;

typedef struct {
    struct __coro_tcb* head;
    struct __coro_tcb* tail;
} coro_queue_t;

void coro_init(void);
int coro_thread_create(coro_thread_t* tid, coro_thread_attr_t* attr,
                       void* (*proc)(void*), void* arg);

int coro_yield(void);

#endif
