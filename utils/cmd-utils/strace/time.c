#include <stdio.h>
#include <sys/timerfd.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/clocknames.h"
#include "xlat/timerfdflags.h"

void print_timespec_t(const struct timespec* ts)
{
    printf("{tv_sec=%lld, ", ts->tv_sec);
    printf("tv_nsec=%ld}", ts->tv_nsec);
}

int print_timespec(struct tcb* tcp, void* addr)
{
    struct timespec ts;

    if (!fetch_mem(tcp, addr, &ts, sizeof(ts))) return -1;

    print_timespec_t(&ts);
    return 0;
}

int print_itimespec(struct tcb* tcp, void* addr)
{
    struct itimerspec its;

    if (!fetch_mem(tcp, addr, &its, sizeof(its))) return -1;

    printf("{it_interval=");
    print_timespec_t(&its.it_interval);
    printf(", it_value=");
    print_timespec_t(&its.it_value);
    printf("}");
    return 0;
}

int trace_timerfd_create(struct tcb* tcp)
{
    printf("timerfd_create(");
    print_flags(tcp->msg_in.u.m_vfs_timerfd.clock_id, &clocknames);
    printf(", ");
    print_flags(tcp->msg_in.u.m_vfs_timerfd.flags, &timerfdflags);
    printf(")");

    return RVAL_DECODED | RVAL_FD;
}

int trace_timerfd_settime(struct tcb* tcp)
{
    MESSAGE* msg;

    if (entering(tcp)) {
        msg = &tcp->msg_in;

        printf("timerfd_settime(%d, ", msg->u.m_vfs_timerfd.fd);
        print_flags(msg->u.m_vfs_timerfd.flags, &timerfdflags);
        printf(", ");
        print_itimespec(tcp, msg->u.m_vfs_timerfd.new_value);
        printf(", ");
    } else {
        msg = &tcp->msg_out;

        print_itimespec(tcp, msg->u.m_vfs_timerfd.old_value);
        printf(")");
    }

    return 0;
}

int trace_timerfd_gettime(struct tcb* tcp)
{
    MESSAGE* msg;

    if (entering(tcp)) {
        msg = &tcp->msg_in;

        printf("timerfd_gettime(%d, ", msg->u.m_vfs_timerfd.fd);
    } else {
        msg = &tcp->msg_out;

        print_itimespec(tcp, msg->u.m_vfs_timerfd.old_value);
        printf(")");
    }

    return 0;
}
