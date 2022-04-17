#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

void ctx_wrapper();

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
    stack_top -= argc > 8 ? argc - 8 : 0;
    stack_top = (unsigned long*)((uintptr_t)stack_top & ~0xf);

    _UC_MACHINE_SET_STACK(ucp, (__greg_t)stack_top);
    _UC_MACHINE_SET_PC(ucp, (__greg_t)ctx_wrapper);
    ucp->uc_mcontext.gregs[_REG_RA] = 0;
    ucp->uc_mcontext.gregs[_REG_X9] = (__greg_t)ucp;   /* s1 = ucp */
    ucp->uc_mcontext.gregs[_REG_X18] = (__greg_t)func; /* s2 = func */

    va_start(ap, argc);
    for (i = 0; i < argc && i < 8; i++)
        ucp->uc_mcontext.gregs[_REG_A0 + i] = va_arg(ap, int);

    for (; i < argc; i++)
        *stack_top++ = va_arg(ap, unsigned long);
    va_end(ap);

    if (stack_top == ucp->uc_stack.ss_sp) {
        _UC_MACHINE_SET_STACK(ucp, 0);
    }
}
