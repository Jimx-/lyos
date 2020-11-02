#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <sys/tls.h>
#include <dlfcn.h>

#ifdef __i386__
#include <asm/ldt.h>
#endif

#include "pthread_internal.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define roundup(value, alignment) (((value) + (alignment)-1) & ~((alignment)-1))

/* TODO: make pthread a dynamic library and use weak alias */
struct tls_tcb* (*__ldso_allocate_tls)(void) = NULL;

pthread_internal_t* __thread_handles[PTHREAD_THREADS_MAX] = {
    &__pthread_initial_thread,
};

static int alloc_stack(const pthread_attr_t* attr,
                       pthread_internal_t* default_thread,
                       pthread_internal_t** new_thread_p, char** stack_addr_p,
                       char** guard_addr_p, size_t* guard_size_p)
{
    pthread_internal_t* thread;
    char *stack_addr, *guard_addr, *map_addr, *res_addr;
    size_t guard_size;
    size_t stack_size;

    if (attr->stackaddr != NULL) {
        thread =
            (pthread_internal_t*)((uintptr_t)attr->stackaddr & -sizeof(void*)) -
            1;
        stack_addr = (char*)attr->stackaddr - attr->stacksize;
        guard_addr = stack_addr;
        guard_size = 0;
    } else {
        guard_size = attr->guardsize;
        stack_size = MIN(roundup(attr->stacksize, 0x1000),
                         PTHREAD_STACK_SIZE_DEFAULT - guard_size);

        thread = default_thread;
        stack_addr = (char*)(thread + 1) - stack_size;
        map_addr = stack_addr - guard_size;
        res_addr =
            mmap(map_addr, stack_size + guard_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (res_addr != map_addr) {
            if (res_addr != MAP_FAILED) {
                munmap(res_addr, stack_size + guard_size);
            }

            return ENOMEM;
        }

        guard_addr = map_addr;
    }

    *new_thread_p = thread;
    *stack_addr_p = stack_addr;
    *guard_addr_p = guard_addr;
    *guard_size_p = guard_size;

    return 0;
}

static inline pthread_internal_t* thread_segment(int slot)
{
    return (pthread_internal_t*)(__pthread_initial_thread_bos -
                                 (slot - 1) * PTHREAD_STACK_SIZE_DEFAULT) -
           1;
}

static int create_thread(const pthread_attr_t* attr, pthread_internal_t** thp,
                         struct tls_tcb** tcbp, char** child_sp)
{
    int slot;
    struct tls_tcb* tcb;
    char *stack_addr, *guard_addr;
    size_t guard_size;
    pthread_internal_t* new_thread;

    for (slot = 1;; slot++) {
        if (slot >= PTHREAD_THREADS_MAX) {
            return EAGAIN;
        }

        if (__thread_handles[slot]) continue;

        if (alloc_stack(attr, thread_segment(slot), &new_thread, &stack_addr,
                        &guard_addr, &guard_size) == 0) {
            break;
        }
    }

    if (!__ldso_allocate_tls)
        __ldso_allocate_tls = dlsym(RTLD_DEFAULT, "__ldso_allocate_tls");

    tcb = __ldso_allocate_tls();
    if (!tcb) return ENOMEM;

    memset(new_thread, 0, sizeof(*new_thread));
    new_thread->thread = slot;
    new_thread->guard_size = guard_size;
    new_thread->guard_addr = guard_addr;
    new_thread->join_state = THREAD_NOT_JOINED;

    tcb->tcb_pthread = new_thread;

    __thread_handles[slot] = new_thread;

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

#ifdef __i386__
    unsigned int gs;
    struct user_desc user_desc;
    __asm__ __volatile__("movw %%gs, %w0" : "=q"(gs));
    user_desc.entry_number = (gs & 0xffff) >> 3;
    user_desc.base_addr = tls;

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

    *thread_out = thread->thread;

    return 0;
}
