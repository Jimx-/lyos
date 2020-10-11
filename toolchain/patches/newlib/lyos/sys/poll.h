#ifndef _SYS_POLL_H_
#define _SYS_POLL_H_

typedef unsigned int nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN   0x0001
#define POLLPRI  0x0002
#define POLLOUT  0x0004
#define POLLERR  0x0008
#define POLLHUP  0x0010
#define POLLNVAL 0x0020

#define POLLRDNORM 0x0040
#define POLLRDBAND 0x0080
#ifndef POLLWRNORM
#define POLLWRNORM 0x0100
#endif
#ifndef POLLWRBAND
#define POLLWRBAND 0x0200
#endif
#ifndef POLLMSG
#define POLLMSG 0x0400
#endif
#ifndef POLLREMOVE
#define POLLREMOVE 0x1000
#endif
#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif

#define POLLFREE     0x4000
#define POLL_NOTIFY  0x8000
#define POLL_ONESHOT 0x10000

#ifdef __cplusplus
extern "C"
{
#endif

    int poll(struct pollfd* fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif
