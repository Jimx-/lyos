#include "ucontextoffsets.h"

    .extern resumecontext

    .global getcontext
    .global setcontext
    .global ctx_wrapper

/* int getcontext(ucontext_t* ucp) */
getcontext:
    movq %rbx, RBX(%rdi)
    movq %rbp, RBP(%rdi)
    movq %r12, R12(%rdi)
    movq %r13, R13(%rdi)
    movq %r14, R14(%rdi)
    movq %r15, R15(%rdi)

    movq %rdi, RDI(%rdi)
    movq %rsi, RSI(%rdi)
    movq %rdx, RDX(%rdi)
    movq %rcx, RCX(%rdi)
    movq %r8, R8(%rdi)
    movq %r9, R9(%rdi)

    movq (%rsp), %rcx
    movq %rcx, RIP(%rdi)
    leaq 8(%rsp), %rcx
    movq %rcx, RSP(%rdi)

    xor %rax, %rax
    ret

/* int setcontext(ucontext_t* ucp) */
setcontext:
    movq RSP(%rdi), %rsp
    movq RBX(%rdi), %rbx
    movq RBP(%rdi), %rbp
    movq R12(%rdi), %r12
    movq R13(%rdi), %r13
    movq R14(%rdi), %r14
    movq R15(%rdi), %r15

    movq RIP(%rdi), %rcx
    push %rcx

    movq RSI(%rdi), %rsi
    movq RDX(%rdi), %rdx
    movq RCX(%rdi), %rcx
    movq R8(%rdi), %r8
    movq R9(%rdi), %r9
    movq RDI(%rdi), %rdi

    xor %rax, %rax
    ret

/* void ctx_wrapper((void *func)(int arg1, ..., argn), arg1, ..., argn,
 *		  ucontext_t *ucp)
 *   Call `func' with arguments and resume ucp->uc_link after `func'
 *   returns.
 */
ctx_wrapper:
    pop %rax
    call *%rax
    mov %rbx, %rsp /* cleanup stack to `ucp' */
    pop %rdi
    call resumecontext
    ret
