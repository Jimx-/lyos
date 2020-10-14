#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_read(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.FD);
    } else {
        if (tcp->msg_out.RETVAL >= 0)
            print_strn(tcp, tcp->msg_in.BUF, tcp->msg_out.RETVAL);
        else
            print_addr((uint64_t)tcp->msg_in.BUF);

        printf(", %d", tcp->msg_in.CNT);
    }

    return 0;
}

int trace_write(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, ", msg->FD);
    print_strn(tcp, msg->BUF, msg->CNT);
    printf(", %d", msg->CNT);

    return RVAL_DECODED;
}
