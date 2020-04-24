#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

void ctx_wrapper(void (*)(void), int, ...);

void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...)
{
    va_list ap;
    unsigned int* stack_top;

    if (ucp == NULL) {
        return;
    } else if ((ucp->uc_stack.ss_sp == NULL) || (ucp->uc_stack.ss_size == 0)) {
        _UC_MACHINE_SET_STACK(ucp, 0);
        return;
    }

    stack_top =
        (unsigned int*)((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
    stack_top = (unsigned int*)((uintptr_t)stack_top & ~0xf);
    stack_top -= (1 + argc + 1); /* func + args + ucp */

    _UC_MACHINE_SET_EBP(ucp, 0);
    _UC_MACHINE_SET_STACK(ucp, (__greg_t)stack_top);
    _UC_MACHINE_SET_PC(ucp, (__greg_t)ctx_wrapper);

    /* copy func + args + ucp */
    *stack_top++ = (uintptr_t)func;

    va_start(ap, argc);
    while (argc-- > 0) {
        *stack_top++ = va_arg(ap, uintptr_t);
    }
    va_end(ap);

    *stack_top = (uintptr_t)ucp;

    /* let ctx_wrapper knows how to cleanup stack */
    _UC_MACHINE_SET_ESI(ucp, (__greg_t)stack_top);

    if (stack_top == ucp->uc_stack.ss_sp) {
        _UC_MACHINE_SET_STACK(ucp, 0);
    }
}
