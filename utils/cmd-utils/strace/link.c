#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

int trace_unlink(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_path(tcp, msg->PATHNAME, msg->NAME_LEN);

    return RVAL_DECODED;
}

int trace_symlink(struct tcb* tcp)
{
    print_path(tcp, tcp->msg_in.u.m_vfs_link.old_path,
               tcp->msg_in.u.m_vfs_link.old_path_len);
    printf(", ");
    print_path(tcp, tcp->msg_in.u.m_vfs_link.new_path,
               tcp->msg_in.u.m_vfs_link.new_path_len);

    return RVAL_DECODED;
}
