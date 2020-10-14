#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_getsetid(struct tcb* tcp)
{
    printf("%d", tcp->msg_in.REQUEST);

    return RVAL_DECODED | RVAL_SPECIAL;
}
