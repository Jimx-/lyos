#include "pthread.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "pthread_internal.h"

#define ALIGN(value, alignment) (((value) + (alignment)-1) & ~((alignment)-1))

static void* alloc_stack(size_t mmap_size, size_t guard_size)
{
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* sp = mmap(NULL, mmap_size, prot, flags, -1, 0);

    if (sp == MAP_FAILED) return NULL;

    return sp;
}

static int create_thread(pthread_attr_t* attr, pthread_internal_t** thp,
                         void** child_sp)
{
    size_t mmap_size;
    char* stack_top;
    if (attr->stackaddr == NULL) {
        mmap_size = ALIGN(attr->stacksize + sizeof(pthread_internal_t), 0x1000);
        attr->guardsize = ALIGN(attr->guardsize, 0x1000);
        attr->stackaddr = alloc_stack(mmap_size, attr->guardsize);

        if (attr->stackaddr == NULL) return EAGAIN;
        stack_top = (char*)attr->stackaddr + mmap_size;
    } else {
        mmap_size = 0;
        stack_top = (char*)attr->stackaddr + attr->stacksize;
    }

    stack_top =
        (char*)(((unsigned)stack_top - sizeof(pthread_internal_t)) & ~0xf);

    pthread_internal_t* thread = (pthread_internal_t*)stack_top;
    memset(thread, 0, sizeof(pthread_internal_t));

    attr->stacksize = stack_top - (char*)attr->stackaddr;
    thread->attr = *attr;
    thread->mmap_size = mmap_size;

    *thp = thread;
    *child_sp = stack_top;

    return 0;
}

static int __pthread_start(void* arg)
{
    pthread_internal_t* thread = (pthread_internal_t*)arg;

    void* result = thread->start_routine(thread->start_arg);

    pthread_exit(result);
    return 0;
}

int pthread_create(pthread_t* thread_out, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg)
{
    pthread_attr_t th_attr;
    if (attr == NULL) {
        pthread_attr_init(&th_attr);
    } else {
        th_attr = *attr;
    }

    pthread_internal_t* thread;
    void* child_sp;
    int retval = create_thread(&th_attr, &thread, &child_sp);
    if (retval) return retval;

    thread->start_routine = start_routine;
    thread->start_arg = arg;

    int flags = CLONE_VM | CLONE_THREAD;
    pid_t pid = clone(__pthread_start, child_sp, flags, (void*)thread);
    if (pid < 0) {
        if (thread->mmap_size) {
            munmap(thread->attr.stackaddr, thread->mmap_size);
        }

        return pid;
    }

    return 0;
}
