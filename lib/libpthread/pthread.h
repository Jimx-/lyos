#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <bits/pthread_types.h>

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

#endif
