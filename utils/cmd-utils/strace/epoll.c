#include <stdio.h>
#include <sys/epoll.h>
#include <inttypes.h>

#include "types.h"
#include "proto.h"

#include "xlat/epollflags.h"
#include "xlat/epollevents.h"
#include "xlat/epollctls.h"

int trace_epoll_create1(struct tcb* tcp)
{
    print_flags(tcp->msg_in.u.m_vfs_epoll.flags, &epollflags);

    return RVAL_DECODED | RVAL_FD;
}

static int print_epoll_event(struct tcb* tcp, void* elem_buf, size_t elem_size,
                             void* data)
{
    const struct epoll_event* ev = elem_buf;

    printf("{");
    print_flags(ev->events, &epollevents);
    printf(", {u32=%" PRIu32 ", u64=%" PRIu64 "}}", ev->data.u32, ev->data.u64);

    return 1;
}

int trace_epoll_ctl(struct tcb* tcp)
{
    MESSAGE* msg = &tcp->msg_in;
    struct epoll_event ev;

    printf("%d, ", msg->u.m_vfs_epoll.epfd);
    print_flags(msg->u.m_vfs_epoll.op, &epollctls);
    printf(", ");
    printf("%d, ", msg->u.m_vfs_epoll.fd);
    if (msg->u.m_vfs_epoll.op == EPOLL_CTL_DEL)
        print_addr((uint64_t)msg->u.m_vfs_epoll.events);
    else if (!fetch_mem(tcp, msg->u.m_vfs_epoll.events, &ev, sizeof(ev)))
        print_addr((uint64_t)msg->u.m_vfs_epoll.events);
    else
        print_epoll_event(tcp, &ev, sizeof(ev), NULL);

    return RVAL_DECODED;
}

int trace_epoll_wait(struct tcb* tcp)
{
    struct epoll_event ev;

    if (entering(tcp)) {
        printf("%d, ", tcp->msg_in.u.m_vfs_epoll.epfd);
    } else {
        if (tcp->msg_out.RETVAL >= 0)
            print_array(tcp, tcp->msg_in.u.m_vfs_epoll.events,
                        tcp->msg_out.RETVAL, &ev, sizeof(ev), fetch_mem,
                        print_epoll_event, NULL);
        else
            print_addr(tcp->msg_in.u.m_vfs_epoll.events);

        printf(", %d, %d", tcp->msg_in.u.m_vfs_epoll.max_events,
               tcp->msg_in.u.m_vfs_epoll.timeout);
    }

    return 0;
}
