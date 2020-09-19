#ifndef _VFS_POLL_H_
#define _VFS_POLL_H_

#include <lyos/types.h>
#include <lyos/wait_queue.h>
#include <uapi/lyos/eventpoll.h>

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

#endif
