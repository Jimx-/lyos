#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "types.h"
#include "proto.h"

#include "xlat/at_flags.h"

int copy_struct_stat(struct tcb* tcp, void* addr, struct stat* stat)
{
    if (!fetch_mem(tcp, addr, stat, sizeof(struct stat))) return -1;

    return 0;
}

void print_struct_stat(struct tcb* tcp, struct stat* stat)
{
    printf("{");
    printf("st_mode=");
    print_mode_t(stat->st_mode);

    if (S_ISBLK(stat->st_mode) || S_ISCHR(stat->st_mode)) {
        printf(", st_rdev=");
        print_dev_t(stat->st_rdev);
    } else {
        printf(", st_size=");
        printf("%llu", (unsigned long long)stat->st_size);
    }

    printf(", ...}");
}

void decode_struct_stat(struct tcb* tcp, void* addr)
{
    struct stat stat;

    if (copy_struct_stat(tcp, addr, &stat) == 0) {
        print_struct_stat(tcp, &stat);
    }
}

int trace_stat(struct tcb* tcp)
{
    if (entering(tcp)) {
        print_path(tcp, tcp->msg_in.PATHNAME, tcp->msg_in.NAME_LEN);
        printf(", ");
    } else {
        decode_struct_stat(tcp, tcp->msg_in.BUF);
    }

    return 0;
}

int trace_lstat(struct tcb* tcp) { return trace_stat(tcp); }

int trace_fstat(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.FD);
    } else {
        decode_struct_stat(tcp, tcp->msg_in.BUF);
    }

    return 0;
}

int trace_fstatat(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    if (entering(tcp)) {
        print_dirfd(msg->u.m_vfs_pathat.dirfd);
        printf(", ");
        print_path(tcp, msg->u.m_vfs_pathat.pathname,
                   msg->u.m_vfs_pathat.name_len);
        printf(", ");
    } else {
        decode_struct_stat(tcp, msg->u.m_vfs_pathat.buf);
        printf(", ");
        print_flags(msg->u.m_vfs_pathat.flags, &at_flags);
    }

    return 0;
}
