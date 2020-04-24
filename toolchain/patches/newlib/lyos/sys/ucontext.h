#ifndef _SYS_UCONTEXT_H_
#define _SYS_UCONTEXT_H_

#include <machine/mcontext.h>
#include <sys/signal.h>

typedef struct __ucontext ucontext_t;

struct __ucontext {
    unsigned int uc_flags;
    ucontext_t* uc_link;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
    stack_t uc_stack;
};

/* uc_flags */
#define _UC_SWAPPED 0x1

#endif
