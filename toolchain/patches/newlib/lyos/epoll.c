#include <errno.h>
#include <sys/epoll.h>

int epoll_create(int size) { return epoll_create1(0); }

int epoll_create1(int flags) { return -ENOSYS; }

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event)
{
    return ENOSYS;
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout)
{
    return ENOSYS;
}
