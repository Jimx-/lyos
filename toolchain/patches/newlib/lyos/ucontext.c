#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

int swapcontext(ucontext_t* oucp, const ucontext_t* ucp)
{
    int retval;

    if ((oucp == NULL) || (ucp == NULL)) {
        errno = EFAULT;
        return -1;
    }

    if (_UC_MACHINE_STACK(ucp) == 0) {
        errno = ENOMEM;
        return -1;
    }

    oucp->uc_flags &= ~_UC_SWAPPED;
    retval = getcontext(oucp);
    if ((retval == 0) && !(oucp->uc_flags & _UC_SWAPPED)) {
        /* don't set context when resuming oucp */
        oucp->uc_flags |= _UC_SWAPPED;
        retval = setcontext(ucp);
    }

    return retval;
}

void resumecontext(ucontext_t* ucp)
{
    if (ucp->uc_link == NULL) {
        exit(0);
    }

    (void)setcontext((const ucontext_t*)ucp->uc_link);
    exit(1);
}
