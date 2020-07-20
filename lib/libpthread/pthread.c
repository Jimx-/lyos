#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include "pthread_internal.h"

static int __pthread_sig_restart = SIGUSR1;

pthread_internal_t __pthread_initial_thread;
char* __pthread_initial_thread_bos = NULL;

static void pthread_handle_sig_restart(int sig);

void pthread_initialize(void)
{
    struct sigaction sa;
    sigset_t mask;

    if (__pthread_initial_thread_bos) return;

    __pthread_initial_thread_bos =
        (char*)(((uintptr_t)CURRENT_SP - 2 * PTHREAD_STACK_SIZE_DEFAULT) &
                ~(PTHREAD_STACK_SIZE_DEFAULT - 1));

    __pthread_initial_thread.pid = getpid();

    sa.sa_handler = pthread_handle_sig_restart;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(__pthread_sig_restart, &sa, NULL);

    /* block restart signal and unblock it on demand */
    sigemptyset(&mask);
    sigaddset(&mask, __pthread_sig_restart);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

static void __pthread_wait_for_restart_signal(pthread_internal_t* self)
{
    sigset_t mask;

    /* unblock restart signal */
    sigprocmask(SIG_SETMASK, NULL, &mask);
    sigdelset(&mask, __pthread_sig_restart);

    self->signal = 0;
    do {
        sigsuspend(&mask);
    } while (self->signal != __pthread_sig_restart);
}

void __pthread_suspend(pthread_internal_t* self)
{
    __pthread_wait_for_restart_signal(self);
}

void __pthread_restart(pthread_internal_t* self)
{
    kill(self->pid, __pthread_sig_restart);
}

static void pthread_handle_sig_restart(int sig)
{
    pthread_internal_t* self = thread_self();

    self->signal = sig;
}

pthread_t pthread_self(void)
{
    pthread_internal_t* thread = thread_self();

    return thread->tid;
}
