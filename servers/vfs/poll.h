#ifndef _VFS_POLL_H_
#define _VFS_POLL_H_

#include <lyos/types.h>
#include <lyos/wait_queue.h>
#include <uapi/lyos/eventpoll.h>
#include <poll.h>

#include "types.h"

#define DEFAULT_POLLMASK (EPOLLIN | EPOLLOUT | EPOLLRDNORM | EPOLLWRNORM)

struct poll_table;
typedef void (*poll_queue_proc)(struct file_desc*, struct wait_queue_head*,
                                struct poll_table*);

struct poll_table {
    poll_queue_proc qproc;
    __poll_t mask;
};

static inline void poll_wait(struct file_desc* filp, struct wait_queue_head* wq,
                             struct poll_table* wait)
{
    if (wait && wait->qproc && wq) {
        wait->qproc(filp, wq, wait);
    }
}

#define __MAP(v, from, to) ((v) & (from) ? (to) : 0)

static inline u16 mangle_poll(__poll_t val)
{
    u16 v = (u16)val;
#define M(x) __MAP(v, EPOLL##x, POLL##x)
    return M(IN) | M(OUT) | M(PRI) | M(ERR) | M(NVAL) | M(RDNORM) | M(RDBAND) |
           M(WRNORM) | M(WRBAND) | M(HUP) | M(RDHUP) | M(MSG);
#undef M
}

static inline __poll_t demangle_poll(u16 val)
{
    __poll_t v = (__poll_t)val;
#define M(x) __MAP(v, POLL##x, EPOLL##x)
    return M(IN) | M(OUT) | M(PRI) | M(ERR) | M(NVAL) | M(RDNORM) | M(RDBAND) |
           M(WRNORM) | M(WRBAND) | M(HUP) | M(RDHUP) | M(MSG);
#undef M
}

#undef __MAP

#endif
