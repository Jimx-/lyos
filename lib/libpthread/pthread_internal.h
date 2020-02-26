#ifndef _PTHREAD_INTERNAL_H_
#define _PTHREAD_INTERNAL_H_

#define PTHREAD_STACK_SIZE_DEFAULT (1 * 1024 * 1024)
#define PTHREAD_GUARD_SIZE_DEFAULT (0x1000)

typedef struct {
    pthread_attr_t attr;
    size_t mmap_size;

    void* (*start_routine)(void*);
    void* start_arg;
} pthread_internal_t;

#endif
