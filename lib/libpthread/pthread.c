#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include "pthread_internal.h"

pthread_internal_t __pthread_initial_thread;
char* __pthread_initial_thread_bos = NULL;

void pthread_initialize(void)
{
    if (__pthread_initial_thread_bos) return;

    __pthread_initial_thread_bos =
        (char*)(((uintptr_t)CURRENT_SP - 2 * PTHREAD_STACK_SIZE_DEFAULT) &
                ~(PTHREAD_STACK_SIZE_DEFAULT - 1));

    __pthread_initial_thread.pid = getpid();
}

pthread_t pthread_self(void)
{
    pthread_internal_t* thread = thread_self();

    return thread->tid;
}

int pthread_equal(pthread_t t1, pthread_t t2) { return t1 == t2; }

int pthread_getcpuclockid(pthread_t thread, clockid_t* clockid)
{
    *clockid = CLOCK_REALTIME;
    return 0;
}

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset)
{
    return sigprocmask(how, set, oldset);
}
