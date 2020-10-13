#include <stdio.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

static void print_pairfd(int fd0, int fd1) { printf("[%d, %d]", fd0, fd1); }

int trace_pipe2(struct tcb* tcp)
{
    if (entering(tcp)) {
        printf("pipe2(");
        return 0;
    } else {
        print_pairfd(tcp->msg_out.u.m_vfs_fdpair.fd0,
                     tcp->msg_out.u.m_vfs_fdpair.fd1);
        printf(", %d)", tcp->msg_in.FLAGS);

        return 0;
    }
}
