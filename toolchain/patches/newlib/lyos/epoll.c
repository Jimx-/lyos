#include <errno.h>
#include <sys/epoll.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <string.h>

int epoll_create(int size)
{
    if (size <= 0) {
        return -EINVAL;
    }

    return epoll_create1(0);
}

int epoll_create1(int flags)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = EPOLL_CREATE1;
    msg.u.m_vfs_epoll.flags = flags;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.FD < 0) {
        errno = -msg.FD;
        return -1;
    }

    return msg.FD;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = EPOLL_CTL;
    msg.u.m_vfs_epoll.epfd = epfd;
    msg.u.m_vfs_epoll.op = op;
    msg.u.m_vfs_epoll.fd = fd;
    msg.u.m_vfs_epoll.events = event;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return 0;
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = EPOLL_WAIT;
    msg.u.m_vfs_epoll.epfd = epfd;
    msg.u.m_vfs_epoll.events = events;
    msg.u.m_vfs_epoll.max_events = maxevents;
    msg.u.m_vfs_epoll.timeout = timeout;

    __asm__ __volatile__("" ::: "memory");

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL < 0) {
        errno = -msg.RETVAL;
        return -1;
    }

    return msg.RETVAL;
}
