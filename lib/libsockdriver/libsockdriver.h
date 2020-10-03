#ifndef _LIBSOCKDRIVER_H_
#define _LIBSOCKDRIVER_H_

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdint.h>
#include <lyos/list.h>
#include <lyos/iov_grant_iter.h>
#include <sys/socket.h>
#include <limits.h>

#include "skbuff.h"
#include "sock.h"

typedef unsigned int sockdriver_worker_id_t;

#define SOCKADDR_MAX (UINT8_MAX + 1)

struct sockdriver_ops;

struct sockdriver_ops {
    int (*sop_bind)(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                    endpoint_t user_endpt, int flags);
    int (*sop_connect)(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                       endpoint_t user_endpt, int flags);
    int (*sop_listen)(struct sock* sock, int backlog);
    int (*sop_accept)(struct sock* sock, struct sockaddr* addr,
                      socklen_t* addrlen, endpoint_t user_endpt, int flags,
                      struct sock** newsockp);
    int (*sop_send_check)(struct sock* sock, size_t len, socklen_t ctl_len,
                          const struct sockaddr* addr, socklen_t addr_len,
                          endpoint_t user_endpt, int flags);
    ssize_t (*sop_send)(struct sock* sock, struct iov_grant_iter* iter,
                        size_t len, const struct sockdriver_data* ctl,
                        socklen_t ctl_len, const struct sockaddr* addr,
                        socklen_t addr_len, endpoint_t user_endpt, int flags);
    int (*sop_recv_check)(struct sock* sock, endpoint_t user_endpt, int flags);
    ssize_t (*sop_recv)(struct sock* sock, struct iov_grant_iter* iter,
                        size_t len, const struct sockdriver_data* ctl,
                        socklen_t* ctl_len, struct sockaddr* addr,
                        socklen_t* addr_len, endpoint_t user_endpt, int flags,
                        int* rflags);
    __poll_t (*sop_poll)(struct sock* sock);
};

struct sockdriver {
    sockid_t (*sd_create)(endpoint_t src, int domain, int type, int protocol,
                          struct sock** sock,
                          const struct sockdriver_ops** ops);
    void (*sd_other)(MESSAGE* msg);
};

void sockdriver_init(void);

void sockdriver_task(const struct sockdriver* sd, size_t num_workers);
sockdriver_worker_id_t sockdriver_worker_id(void);
void sockdriver_sleep(void);
void sockdriver_yield(void);
void sockdriver_wakeup(sockdriver_worker_id_t tid);
void sockdriver_set_workers(size_t num_workers);

void sockdriver_clone(struct sock* sock, struct sock* newsock, sockid_t newid);

void sockdriver_suspend(struct sock* sock, unsigned int event);
void sockdriver_fire(struct sock* sock, unsigned int mask);

int sockdriver_copyin(const struct sockdriver_data* data, size_t off, void* ptr,
                      size_t len);
int sockdriver_copyout(const struct sockdriver_data* data, size_t off,
                       void* ptr, size_t len);

int skb_alloc(struct sock* sock, size_t data_len, struct sk_buff** skbp);
void skb_free(struct sk_buff* skb);
int skb_copy_from(struct sk_buff* skb, size_t off,
                  const struct sockdriver_data* data, size_t data_off,
                  size_t len);
int skb_copy_to(struct sk_buff* skb, size_t off,
                const struct sockdriver_data* data, size_t data_off,
                size_t len);
int skb_copy_from_iter(struct sk_buff* skb, size_t off,
                       struct iov_grant_iter* iter, size_t len);
int skb_copy_to_iter(struct sk_buff* skb, size_t off,
                     struct iov_grant_iter* iter, size_t len);

#endif
