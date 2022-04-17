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
    } else if ((ucp->uc_stack.ss_sp == NULL) ||
               (ucp->uc_stack.ss_size < MINSIGSTKSZ)) {
        _UC_MACHINE_SET_STACK(ucp, 0);
        return;
    }

    stack_top = (unsigned long*)((uintptr_t)ucp->uc_stack.ss_sp +
                                 ucp->uc_stack.ss_size);
    stack_top = (unsigned long*)((uintptr_t)stack_top & ~0xf);

    if (argc > 8) stack_top -= argc - 8;

    _UC_MACHINE_SET_STACK(ucp, (unsigned long)stack_top);
    _UC_MACHINE_SET_PC(ucp, (unsigned long)func);
    _UC_MACHINE_SET_LR(ucp, (unsigned long)ctx_wrapper);
    _UC_MACHINE_SET_FP(ucp, 0);
    ucp->uc_mcontext.gregs[_REG_X21] = (unsigned long)ucp;

    va_start(ap, argc);

    for (i = 0; i < argc && i < 8; i++) {
        ucp->uc_mcontext.gregs[i] = va_arg(ap, uintptr_t);
    }

    for (; i < argc; i++) {
        *stack_top++ = va_arg(ap, uintptr_t);
    }

    va_end(ap);

    if (stack_top == ucp->uc_stack.ss_sp) {
        _UC_MACHINE_SET_STACK(ucp, 0);
    }
}
