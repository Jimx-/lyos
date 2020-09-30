#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

static int dup12(struct tcb* tcp, int newfd)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d", msg->FD);

    if (newfd >= 0) {
        printf(", ");
        printf("%d", newfd);
    }

    return RVAL_DECODED | RVAL_FD;
}

int trace_dup(struct tcb* tcp)
{
    int retval;
    int newfd;

    newfd = tcp->msg_in.NEWFD;

    if (newfd >= 0)
        printf("dup2(");
    else
        printf("dup(");

    retval = dup12(tcp, newfd);
    printf(")");

    return retval;
}
