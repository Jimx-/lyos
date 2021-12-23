#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

void ctx_wrapper(void (*)(void), int, ...);

void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...)
{
    int i;
    va_list ap;
    unsigned long* stack_top;

    if (ucp == NULL) {
        return;
    } else if ((ucp->uc_stack.ss_sp == NULL) || (ucp->uc_stack.ss_size == 0)) {
        _UC_MACHINE_SET_STACK(ucp, 0);
        return;
    }

    stack_top = (unsigned long*)((uintptr_t)ucp->uc_stack.ss_sp +
                                 ucp->uc_stack.ss_size);
    stack_top = (unsigned long*)((uintptr_t)stack_top & ~0xf);
    stack_top -= (1 + (argc > 6 ? argc - 6 : 0) + 1); /* args */

    _UC_MACHINE_SET_RBP(ucp, 0);
    _UC_MACHINE_SET_STACK(ucp, (__greg_t)stack_top);
    _UC_MACHINE_SET_PC(ucp, (__greg_t)ctx_wrapper);

    /* copy func + args + ucp */
    *stack_top++ = (uintptr_t)func;

    va_start(ap, argc);
    for (i = 0; i < argc; ++i)
        switch (i) {
        case 0:
            ucp->uc_mcontext.gregs[_REG_RDI] = va_arg(ap, unsigned long);
            break;
        case 1:
            ucp->uc_mcontext.gregs[_REG_RSI] = va_arg(ap, unsigned long);
            break;
        case 2:
            ucp->uc_mcontext.gregs[_REG_RDX] = va_arg(ap, unsigned long);
            break;
        case 3:
            ucp->uc_mcontext.gregs[_REG_RCX] = va_arg(ap, unsigned long);
            break;
        case 4:
            ucp->uc_mcontext.gregs[_REG_R8] = va_arg(ap, unsigned long);
            break;
        case 5:
            ucp->uc_mcontext.gregs[_REG_R9] = va_arg(ap, unsigned long);
            break;
        default:
            /* Put value on stack.  */
            *stack_top++ = va_arg(ap, unsigned long);
            break;
        }

    va_end(ap);

    *stack_top = (uintptr_t)ucp;

    /* let ctx_wrapper knows how to cleanup stack */
    ucp->uc_mcontext.gregs[_REG_RBX] = (unsigned long)stack_top;

    if (stack_top == ucp->uc_stack.ss_sp) {
        _UC_MACHINE_SET_STACK(ucp, 0);
    }
}
