#include "ucontextoffsets.h"

    .extern resumecontext
    
    .global getcontext
    .global setcontext
    .global ctx_wrapper

/* int getcontext(ucontext_t* ucp) */
getcontext:
    mov 4(%esp), %edx
    cmp $0, %edx
    jne 1f
    xor %eax, %eax
    dec %eax
    ret /* return -1 */

1:
    pop PC(%edx) /* save return address */
    mov %esp, SP(%edx)
    mov %ebp, BP(%edx)
    mov %esi, SI(%edx)
    mov %edi, DI(%edx)
    mov %ebx, BX(%edx)
    mov %ecx, CX(%edx)
    xor %eax, %eax
    jmp *PC(%edx) /* ret */

/* int setcontext(ucontext_t* ucp) */
setcontext:
    mov 4(%esp), %edx
    cmp $0, %edx
    jne 1f
    xor %eax, %eax
    dec %eax
    ret /* return -1 */

1:
    mov CX(%edx), %ecx
    mov BX(%edx), %ebx
    mov DI(%edx), %edi
    mov SI(%edx), %esi
    mov BP(%edx), %ebp
    mov SP(%edx), %esp
    xor %eax, %eax
    jmp *PC(%edx)

/* void ctx_wrapper((void *func)(int arg1, ..., argn), arg1, ..., argn,
 *		  ucontext_t *ucp)
 *   Call `func' with arguments and resume ucp->uc_link after `func'
 *   returns.
 */
ctx_wrapper:
    pop %eax
    call *%eax
    mov %esi, %esp /* cleanup stack to `ucp' */
    call resumecontext
    ret
