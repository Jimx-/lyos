#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <bits/pthread_types.h>
#include <sys/types.h>

#define PTHREAD_MUTEX_NORMAL		0
#define PTHREAD_MUTEX_RECURSIVE		1
#define PTHREAD_MUTEX_ERRORCHECK	2

typedef struct {
	int private;
} pthread_mutex_t;

#define PTHREAD_MUTEX_INITIALIZER { { ((PTHREAD_MUTEX_NORMAL & 3) << 14) } }

typedef long pthread_mutexattr_t;

typedef struct {
	unsigned int state;
} pthread_cond_t;

typedef long pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER  { { 0 } }

#define PTHREAD_CREATE_DETACHED  0x00000001
#define PTHREAD_CREATE_JOINABLE  0x00000000

#define PTHREAD_INHERIT_SCHED    0
#define PTHREAD_EXPLICIT_SCHED   1

#define PTHREAD_SCOPE_SYSTEM     0
#define PTHREAD_SCOPE_PROCESS    1

int pthread_attr_init(pthread_attr_t*);
int pthread_create(pthread_t*, const pthread_attr_t*,
    void* (*start_routine)(void*), void*);
int pthread_exit(void* value_ptr);

int pthread_cond_init(pthread_cond_t*, const pthread_condattr_t*);
int pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);


int pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);

#endif
