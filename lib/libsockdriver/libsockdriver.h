#ifndef _LIBSOCKDRIVER_H_
#define _LIBSOCKDRIVER_H_

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdint.h>
#include <lyos/list.h>
#include <sys/socket.h>
#include <limits.h>

typedef int32_t sockid_t;
typedef unsigned int sockdriver_worker_id_t;

#define SOCKADDR_MAX (UINT8_MAX + 1)

struct sockdriver_ops;

/* Socket events. */
#define SEV_BIND    0x01
#define SEV_CONNECT 0x02
#define SEV_ACCEPT  0x04
#define SEV_SEND    0x08
#define SEV_RECV    0x10
#define SEV_CLOSE   0x20

/* Socket flags. */
#define SFL_SHUT_RD 0x01
#define SFL_SHUT_WR 0x02

struct sockdriver_sock {
    sockid_t id;
    unsigned int flags;
    int domain;
    int type;
    unsigned int sock_opt;

    struct list_head hash;
    struct list_head wq;

    const struct sockdriver_ops* ops;
};

#define sockdriver_get_sockid(sock)        ((sock)->id)
#define sockdriver_get_type(sock)          ((sock)->type)
#define sockdriver_is_listening(sock)      (!!((sock)->sock_opt & SO_ACCEPTCONN))
#define sockdriver_is_shutdown(sock, mask) ((sock)->flags & (mask))

struct sockdriver_ops {
    int (*sop_bind)(struct sockdriver_sock* sock, struct sockaddr* addr,
                    size_t addrlen, endpoint_t user_endpt, int flags);
    int (*sop_connect)(struct sockdriver_sock* sock, struct sockaddr* addr,
                       size_t addrlen, endpoint_t user_endpt, int flags);
    int (*sop_listen)(struct sockdriver_sock* sock, int backlog);
    int (*sop_accept)(struct sockdriver_sock* sock, struct sockaddr* addr,
                      socklen_t* addrlen, endpoint_t user_endpt, int flags,
                      struct sockdriver_sock** newsockp);
};

typedef sockid_t (*sockdriver_socket_cb_t)(endpoint_t src, int domain, int type,
                                           int protocol,
                                           struct sockdriver_sock** sock,
                                           const struct sockdriver_ops** ops);

void sockdriver_init(sockdriver_socket_cb_t socket_cb);

void sockdriver_task(size_t num_workers);
sockdriver_worker_id_t sockdriver_worker_id(void);
void sockdriver_sleep(void);
void sockdriver_yield(void);
void sockdriver_wakeup(sockdriver_worker_id_t tid);
void sockdriver_set_workers(size_t num_workers);

void sockdriver_clone(struct sockdriver_sock* sock,
                      struct sockdriver_sock* newsock, sockid_t newid);

void sockdriver_suspend(struct sockdriver_sock* sock, unsigned int event);
void sockdriver_fire(struct sockdriver_sock* sock, unsigned int mask);

#endif
