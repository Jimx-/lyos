#ifndef _LIBCORO_H_
#define _LIBCORO_H_

#include <sys/types.h>

typedef int coro_thread_t;

struct __coro_tcb;

typedef struct {
    size_t stacksize;
    void* stackaddr;
} coro_attr_t;

typedef struct {
    struct __coro_tcb* head;
    struct __coro_tcb* tail;
} coro_queue_t;

typedef struct {
    coro_thread_t owner;
    coro_queue_t wait_queue;
} coro_mutex_t;
typedef int coro_mutexattr_t;

typedef struct {
    coro_queue_t wait_queue;
} coro_cond_t;
typedef int coro_condattr_t;

typedef struct {
    size_t readers;
    coro_thread_t writer;
    coro_mutex_t reader_queue;
    coro_mutex_t mutex;
    coro_cond_t writer_queue;
} coro_rwlock_t;

/* thread.c */
void coro_init(void);
int coro_thread_create(coro_thread_t* tid, coro_attr_t* attr,
                       void* (*proc)(void*), void* arg);
coro_thread_t coro_self(void);
int coro_join(coro_thread_t thread, void** value);

/* attr.c */
int coro_attr_init(coro_attr_t* attr);
int coro_attr_setstacksize(coro_attr_t* attr, size_t stacksize);

/* scheduler.c */
int coro_yield(void);
void coro_yield_all(void);

/* mutex.c */
int coro_mutex_init(coro_mutex_t* mutex, coro_mutexattr_t* attr);
int coro_mutex_trylock(coro_mutex_t* mutex);
int coro_mutex_lock(coro_mutex_t* mutex);
int coro_mutex_unlock(coro_mutex_t* mutex);

/* condvar.c */
int coro_cond_init(coro_cond_t* cond, coro_condattr_t* attr);
int coro_cond_signal(coro_cond_t* cond);
int coro_cond_broadcast(coro_cond_t* cond);
int coro_cond_wait(coro_cond_t* cond, coro_mutex_t* mutex);

/* rwlock.c */
int coro_rwlock_init(coro_rwlock_t* rwlock);
int coro_rwlock_rdlock(coro_rwlock_t* rwlock);
int coro_rwlock_wrlock(coro_rwlock_t* rwlock);
int coro_rwlock_unlock(coro_rwlock_t* rwlock);

#endif
