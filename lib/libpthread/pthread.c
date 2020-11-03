#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include "pthread_internal.h"

pthread_internal_t __pthread_initial_thread = {.tid = -1};

pthread_t pthread_self(void) { return (pthread_t)__libc_get_tls_tcb(); }

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
