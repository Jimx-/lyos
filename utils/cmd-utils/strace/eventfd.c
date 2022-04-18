#include <stdio.h>
#include <sys/eventfd.h>

#include "types.h"
#include "proto.h"

#include "xlat/efd_flags.h"

int trace_eventfd(struct tcb* tcp)
{
    printf("%u, ", tcp->msg_in.CNT);
    print_flags(tcp->msg_in.FLAGS, &efd_flags);

    return RVAL_DECODED | RVAL_FD;
}
