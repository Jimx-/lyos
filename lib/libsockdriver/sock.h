#ifndef _LIBSOCKDRIVER_SOCK_H_
#define _LIBSOCKDRIVER_SOCK_H_

#include <lyos/const.h>
#include <lyos/types.h>
#include <lyos/list.h>
#include <time.h>

typedef int32_t sockid_t;

/* Socket events. */
#define SEV_BIND    0x01
#define SEV_CONNECT 0x02
#define SEV_ACCEPT  0x04
#define SEV_SEND    0x08
#define SEV_RECV    0x10
#define SEV_CLOSE   0x20

/* Socket flags. */
#define SFL_SHUT_RD    0x001
#define SFL_SHUT_WR    0x002
#define SFL_DBG        0x004
#define SFL_PASSCRED   0x008
#define SFL_LOCALROUTE 0x010
#define SFL_REUSE      0x020
#define SFL_KEEPOPEN   0x040
#define SFL_URGINLINE  0x080
#define SFL_ACCEPTCONN 0x100
#define SFL_LINGER     0x200
#define SFL_BROADCAST  0x400

struct sock_cred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

struct sock {
    sockid_t id;
    unsigned int flags;
    int domain;
    int type;
    int protocol;
    int err;

    int peek_off;
    int rcvlowat;
    int sndbuf;
    clock_t linger;

    struct sock_cred peercred;

    struct list_head hash;
    struct list_head wq;
    struct sk_buff_head recvq;

    endpoint_t sel_endpoint;
    __poll_t sel_mask;
    __poll_t sel_oneshot_mask;

    const struct sockdriver_ops* ops;
};

struct sockdriver_data {
    endpoint_t endpoint;
    mgrant_id_t grant;
    size_t len;
};

#define sock_sockid(sock)            ((sock)->id)
#define sock_domain(sock)            ((sock)->domain)
#define sock_type(sock)              ((sock)->type)
#define sock_flags(sock)             ((sock)->flags)
#define sock_protocol(sock)          ((sock)->protocol)
#define sock_recvq(sock)             ((sock)->recvq)
#define sock_peercred(sock)          ((sock)->peercred)
#define sock_is_listening(sock)      (!!((sock)->flags & SFL_ACCEPTCONN))
#define sock_is_shutdown(sock, mask) ((sock)->flags & (mask))

struct sock* sock_get(sockid_t id);
void sock_free(struct sock* sock);

static inline int sock_error(struct sock* sock)
{
    int retval = sock->err;
    sock->err = 0;
    return retval;
}

static inline void sock_set_error(struct sock* sock, int err)
{
    sock->err = err;
}

static inline int sock_rcvlowat(const struct sock* sock, int waitall, int len)
{
    int v = waitall ? len : min(sock->rcvlowat, len);

    return v ?: 1;
}

static inline int sock_peek_offset(struct sock* sock, int flags)
{
    if (flags & MSG_PEEK) {
        return sock->peek_off;
    }

    return 0;
}

#endif
