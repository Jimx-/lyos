#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_umask(struct tcb* tcp)
{
    print_mode_t(tcp->msg_in.MODE);

    return RVAL_DECODED | RVAL_SPECIAL;
}
