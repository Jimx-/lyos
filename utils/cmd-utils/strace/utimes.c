#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "proto.h"

#include "xlat/at_flags.h"

int trace_utimensat(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;
    struct timespec atime, mtime;

    print_dirfd(msg->u.m_vfs_utimensat.fd);
    printf(", ");

    print_path(tcp, msg->u.m_vfs_utimensat.pathname,
               msg->u.m_vfs_utimensat.name_len);
    printf(", ");

    atime.tv_sec = msg->u.m_vfs_utimensat.atime;
    atime.tv_nsec = msg->u.m_vfs_utimensat.ansec;
    mtime.tv_sec = msg->u.m_vfs_utimensat.mtime;
    mtime.tv_nsec = msg->u.m_vfs_utimensat.mnsec;

    printf("[");
    print_timespec_t(&atime);
    printf(", ");
    print_timespec_t(&mtime);
    printf("], ");

    print_flags(msg->u.m_vfs_utimensat.flags, &at_flags);

    return RVAL_DECODED | RVAL_SPECIAL;
}
