#include <stdio.h>
#include <fcntl.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/fcntlcmds.h"
#include "xlat/fdflags.h"

static int print_fcntl(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;
    int cmd = msg->REQUEST;

    switch (cmd) {
    case F_SETFD:
        printf(", ");
        print_flags(msg->BUF_LEN, &fdflags);
        break;
    }
    return RVAL_DECODED;
}

int trace_fcntl(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.FD);
        print_xval(tcp->msg_in.REQUEST, &fcntlcmds);
    }

    return print_fcntl(tcp);
}
