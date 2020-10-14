#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_exec(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", %p", msg->BUF);

    return RVAL_DECODED;
}
