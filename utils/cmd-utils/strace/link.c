#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_unlink(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("unlink(");
    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(")");

    return RVAL_DECODED;
}
