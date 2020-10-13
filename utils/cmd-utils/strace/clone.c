#include <stdio.h>

#include "types.h"

int trace_fork(struct tcb* tcp)
{
    printf("fork()");

    return RVAL_DECODED;
}
