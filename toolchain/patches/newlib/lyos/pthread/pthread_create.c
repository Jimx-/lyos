#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdint.h>
#include <sched.h>
#include <sys/tls.h>

#ifdef __i386__
#include <asm/ldt.h>
#endif

#include "pthread_internal.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

void* __ldso_allocate_tls(void)
    __attribute__((weak, alias("___ldso_allocate_tls")));
void* ___ldso_allocate_tls(void) { return NULL; }

#define roundup(value, alignment) (((value) + (alignment)-1) & ~((alignment)-1))

static int alloc_stack(const pthread_attr_t* attr,
                       pthread_internal_t** new_thread_p, char** stack_addr_p,
                       char** guard_addr_p, size_t* guard_size_p,
                       size_t* mmap_size_p)
{
    pthread_internal_t* thread;
    char *stack_addr, *guard_addr, *map_addr;
    size_t guard_size;
    size_t stack_size;
    size_t mmap_size;

    if (attr->stackaddr != NULL) {
        thread =
            (pthread_internal_t*)((uintptr_t)attr->stackaddr & -sizeof(void*)) -
            1;
        stack_addr = (char*)attr->stackaddr - attr->stacksize;
        guard_addr = stack_addr;
        guard_size = 0;
        mmap_size = 0;
    } else {
        guard_size = attr->guardsize;
        stack_size = MIN(roundup(attr->stacksize, 0x1000),
                         PTHREAD_STACK_SIZE_DEFAULT - guard_size);

        mmap_size = stack_size + guard_size;
        map_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (map_addr == MAP_FAILED) return ENOMEM;

        guard_addr = map_addr;
        stack_addr = map_addr + guard_size;

        thread =
            (pthread_internal_t*)(stack_addr + stack_size - sizeof(*thread));
        if ((uintptr_t)thread % 16)
            thread = (pthread_internal_t*)((uintptr_t)thread -
                                           ((uintptr_t)thread % 16));
    }

    *new_thread_p = thread;
    *stack_addr_p = stack_addr;
    *guard_addr_p = guard_addr;
    *guard_size_p = guard_size;
    *mmap_size_p = mmap_size;

    return 0;
}

static int create_thread(const pthread_attr_t* attr, pthread_internal_t** thp,
                         struct tls_tcb** tcbp, char** child_sp)
{
    struct tls_tcb* tcb;
    char *stack_addr, *guard_addr;
    size_t guard_size, mmap_size;
    pthread_internal_t* new_thread;
    int retval;

    retval = alloc_stack(attr, &new_thread, &stack_addr, &guard_addr,
                         &guard_size, &mmap_size);
    if (retval) return retval;

    tcb = __ldso_allocate_tls();
    if (!tcb) return ENOMEM;

    memset(new_thread, 0, sizeof(*new_thread));
    new_thread->guard_size = guard_size;
    new_thread->guard_addr = guard_addr;
    new_thread->mmap_size = mmap_size;
    new_thread->join_state = THREAD_NOT_JOINED;

    tcb->tcb_pthread = new_thread;

    *thp = new_thread;
    *tcbp = tcb;
    *child_sp = (char*)new_thread;

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
    pthread_internal_t* thread;
    struct tls_tcb* tcb;
    char* child_sp;
    void* tls;
    pthread_attr_t th_attr;
    pid_t tid;
    int clone_flags;
    int retval;

    if (attr == NULL) {
        pthread_attr_init(&th_attr);
    } else {
        th_attr = *attr;
    }

    retval = create_thread(&th_attr, &thread, &tcb, &child_sp);
    if (retval) return retval;

    thread->start_routine = start_routine;
    thread->start_arg = arg;

    tls = tcb;

#ifdef __HAVE_TLS_VARIANT_1
    tls += sizeof(struct tls_tcb);
#endif

#ifdef __i386__
    unsigned int gs;
    struct user_desc user_desc;
    __asm__ __volatile__("movw %%gs, %w0" : "=q"(gs));
    user_desc.entry_number = (gs & 0xffff) >> 3;
    user_desc.base_addr = (unsigned int)tls;

    tls = &user_desc;
#endif

    clone_flags = CLONE_VM | CLONE_THREAD | CLONE_SETTLS | CLONE_PARENT_SETTID |
                  CLONE_CHILD_CLEARTID;
    tid = clone(__pthread_start, child_sp, clone_flags, (void*)thread,
                &thread->tid, tls, &thread->tid);
    if (tid < 0) {
        if (thread->guard_size) {
            munmap(thread->guard_addr,
                   (char*)(thread + 1) - thread->guard_addr);
        }

        return tid;
    }

    *thread_out = (pthread_t)tcb;

    return 0;
}
