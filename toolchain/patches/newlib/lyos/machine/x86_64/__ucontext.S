#include "ucontextoffsets.h"

    .extern resumecontext

    .global getcontext
    .global setcontext
    .global ctx_wrapper

/* int getcontext(ucontext_t* ucp) */
getcontext:
    ret

/* int setcontext(ucontext_t* ucp) */
setcontext:
    ret

/* void ctx_wrapper((void *func)(int arg1, ..., argn), arg1, ..., argn,
 *		  ucontext_t *ucp)
 *   Call `func' with arguments and resume ucp->uc_link after `func'
 *   returns.
 */
ctx_wrapper:
    ret
