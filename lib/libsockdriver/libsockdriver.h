#ifndef _LIBSOCKDRIVER_H_
#define _LIBSOCKDRIVER_H_

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdint.h>
#include <lyos/list.h>

typedef int32_t sockid_t;
typedef unsigned int sockdriver_worker_id_t;

struct sockdriver_ops;

struct sockdriver_sock {
    sockid_t id;
    int domain;
    int type;

    struct list_head hash;

    const struct sockdriver_ops* ops;
};

struct sockdriver_ops {};

typedef sockid_t (*sockdriver_socket_cb_t)(endpoint_t src, int domain, int type,
                                           int protocol,
                                           struct sockdriver_sock** sock,
                                           const struct sockdriver_ops** ops);

void sockdriver_init(sockdriver_socket_cb_t socket_cb);
void sockdriver_process(MESSAGE* msg);

void sockdriver_async_task(void);
sockdriver_worker_id_t sockdriver_async_worker_id(void);
void sockdriver_async_sleep(void);
void sockdriver_async_wakeup(sockdriver_worker_id_t tid);
void sockdriver_async_set_workers(size_t num_workers);

#endif
