#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

void resumecontext(ucontext_t* ucp);

void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...)
{
    va_list ap;
    unsigned long* stack_top;
    int i;

    if (ucp == NULL) {
        return;
    } else if ((ucp->uc_stack.ss_sp == NULL) || (ucp->uc_stack.ss_size == 0)) {
        _UC_MACHINE_SET_STACK(ucp, 0);
        return;
    }

    stack_top = (unsigned long*)((uintptr_t)ucp->uc_stack.ss_sp +
                                 ucp->uc_stack.ss_size);
    stack_top -= 1 + (argc > 8 ? argc - 8 : 0);
    stack_top = (unsigned long*)((uintptr_t)stack_top & ~0xf);

    _UC_MACHINE_SET_STACK(ucp, (__greg_t)stack_top);
    _UC_MACHINE_SET_PC(ucp, (__greg_t)func);
    ucp->uc_mcontext.gregs[_REG_RA] = resumecontext;

    *stack_top++ = 0;

    va_start(ap, argc);
    for (i = 0; i < argc && i < 8; i++)
        ucp->uc_mcontext.gregs[_REG_A0 + i] = va_arg(ap, int);

    for (; i < argc; i++)
        *stack_top++ = va_arg(ap, unsigned long);
    va_end(ap);
}
