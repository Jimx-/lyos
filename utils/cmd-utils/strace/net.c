#include <stdio.h>
#include <sys/socket.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/addrfams.h"
#include "xlat/socktypes.h"
#include "xlat/sock_flags.h"

static void print_pairfd(int fd0, int fd1) { printf("[%d, %d]", fd0, fd1); }

int trace_pipe2(struct tcb* tcp)
{
    if (entering(tcp)) {
        return 0;
    } else {
        print_pairfd(tcp->msg_out.u.m_vfs_fdpair.fd0,
                     tcp->msg_out.u.m_vfs_fdpair.fd1);
        printf(", %d", tcp->msg_in.FLAGS);

        return 0;
    }
}

static void print_sock_type(int flags)
{
    const char* str;

    str = xlookup(&socktypes, flags & 0xf);
    flags &= ~0xf;
    if (str) {
        printf("%s", str);

        if (flags) putchar('|');
    }

    if (!str || flags) print_flags(flags, &sock_type_flags);
}

int trace_socket(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    print_flags(msg->u.m_vfs_socket.domain, &addrfams);
    printf(", ");
    print_sock_type(msg->u.m_vfs_socket.type);
    printf(", ");
    printf("%d", msg->u.m_vfs_socket.protocol);

    return RVAL_DECODED | RVAL_FD;
}

static void do_bindconn(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;
    size_t addrlen = msg->u.m_vfs_bindconn.addr_len;

    printf("%d, ", msg->u.m_vfs_bindconn.sock_fd);
    decode_sockaddr(tcp, msg->u.m_vfs_bindconn.addr, addrlen);
    printf(", %d", addrlen);
}
int trace_bind(struct tcb* tcp)
{
    do_bindconn(tcp);

    return RVAL_DECODED;
}

int trace_connect(struct tcb* tcp)
{
    do_bindconn(tcp);

    return RVAL_DECODED;
}

int trace_listen(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;

    printf("%d, %d", msg->u.m_vfs_listen.sock_fd, msg->u.m_vfs_listen.backlog);

    return RVAL_DECODED;
}

int trace_accept(struct tcb* tcp)
{
    int ulen, rlen;
    MESSAGE* msg = &tcp->msg_in;

    if (entering(tcp)) {
        printf("%d, ", msg->u.m_vfs_bindconn.sock_fd);

        return RVAL_FD;
    }

    ulen = msg->u.m_vfs_bindconn.addr_len;
    rlen = tcp->msg_out.CNT;

    if (tcp->msg_out.FD < 0) {
        print_addr((uint64_t)msg->u.m_vfs_bindconn.addr);
        printf(", [%d]", ulen);
    } else {
        decode_sockaddr(tcp, msg->u.m_vfs_bindconn.addr,
                        ulen > rlen ? rlen : ulen);
        if (ulen != rlen)
            printf(", [%d->%d]", ulen, rlen);
        else
            printf(", [%d]", rlen);
    }

    return RVAL_DECODED | RVAL_FD;
}
