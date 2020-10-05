#if !defined _PTHREAD_H_
#error "Never include <bits/pthreadtypes.h> directly; use <pthread.h> instead."
#endif

#ifndef _BITS_PTHREADTYPES_H_
#define _BITS_PTHREADTYPES_H_

#include <sys/sched.h>

typedef struct {
    int detachstate;
    int schedpolicy;
    struct sched_param schedparam;
    int inheritsched;
    int scope;
    size_t guardsize;
    int stackaddr_set;
    void* stackaddr;
    size_t stacksize;
} pthread_attr_t;

typedef struct {
    unsigned int state;
    unsigned int owner;
} pthread_mutex_t;

typedef long pthread_mutexattr_t;

typedef struct {
    unsigned int state;
} pthread_cond_t;

typedef long pthread_condattr_t;

typedef struct {
    int state;
    int owner;

    pthread_mutex_t pending_mutex;
    unsigned int pending_reader_count;
    unsigned int pending_writer_count;
    /*struct list_head pending_writers;
    struct list_head pending_readers;*/

    unsigned int pending_reader_serial;
    unsigned int pending_writer_serial;
} pthread_rwlock_t;

typedef long pthread_rwlockattr_t;

typedef struct {
    unsigned int waiting;
    unsigned int count;
} pthread_barrier_t;

typedef long pthread_barrierattr_t;

typedef int pthread_key_t;

typedef int pthread_once_t;

#endif
