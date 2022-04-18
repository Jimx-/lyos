#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "proto.h"

#include "xlat/access_modes.h"

int trace_access(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);
    printf(", ");
    print_flags(msg->MODE, &access_modes);

    return RVAL_DECODED;
}
