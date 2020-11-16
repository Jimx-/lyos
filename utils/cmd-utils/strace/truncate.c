#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "uapi/lyos/ipc.h"
#include "xlat.h"
#include "proto.h"

int trace_ftruncate(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, ", msg->u.m_vfs_truncate.fd);
    printf("%lu", msg->u.m_vfs_truncate.offset);

    return RVAL_DECODED | RVAL_SPECIAL;
}
