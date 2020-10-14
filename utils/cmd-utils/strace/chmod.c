#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_chmod(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", ");
    print_mode_t(msg->MODE);

    return RVAL_DECODED;
}
