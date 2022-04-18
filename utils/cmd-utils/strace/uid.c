#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "proto.h"

#include "xlat/at_flags.h"

void print_uid(unsigned int uid)
{
    if ((uid_t)-1U == (uid_t)uid)
        printf("-1");
    else
        printf("%u", (uid_t)uid);
}

int trace_fchown(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_dirfd(msg->u.m_vfs_fchownat.fd);
    printf(", ");

    print_uid(msg->u.m_vfs_fchownat.owner);
    printf(", ");

    print_uid(msg->u.m_vfs_fchownat.group);

    return RVAL_DECODED | RVAL_SPECIAL;
}

int trace_fchownat(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_dirfd(msg->u.m_vfs_fchownat.fd);
    printf(", ");

    print_path(tcp, msg->u.m_vfs_fchownat.pathname,
               msg->u.m_vfs_fchownat.name_len);
    printf(", ");

    print_uid(msg->u.m_vfs_fchownat.owner);
    printf(", ");

    print_uid(msg->u.m_vfs_fchownat.group);
    printf(", ");

    print_flags(msg->u.m_vfs_fchownat.flags, &at_flags);

    return RVAL_DECODED | RVAL_SPECIAL;
}
