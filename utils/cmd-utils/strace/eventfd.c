#include <stdio.h>
#include <sys/eventfd.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/efd_flags.h"

int trace_eventfd(struct tcb* tcp)
{
    printf("eventfd(%u, ", tcp->msg_in.CNT);
    print_flags(tcp->msg_in.FLAGS, &efd_flags);
    printf(")");

    return RVAL_DECODED | RVAL_FD;
}
