#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_getdents(struct tcb* tcp)
{
    printf("%d, %p, %d", tcp->msg_in.FD, tcp->msg_in.BUF, tcp->msg_in.CNT);

    return RVAL_DECODED;
}
