#include <stdio.h>
#include <sys/stat.h>
#include <sys/ptrace.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

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
        printf("%llu", stat->st_size);
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
        printf("stat(");
        print_path(tcp, tcp->msg_in.PATHNAME, tcp->msg_in.NAME_LEN);
        printf(", ");
    } else {
        decode_struct_stat(tcp, tcp->msg_in.BUF);
        printf(")");
    }

    return 0;
}

int trace_fstat(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("fstat(%d, ", tcp->msg_in.FD);
    } else {
        decode_struct_stat(tcp, tcp->msg_in.BUF);
        printf(")");
    }

    return 0;
}
