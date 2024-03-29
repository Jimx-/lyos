#define _GNU_SOURCE
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>
#include <lyos/mgrant.h>
#include <lyos/sysutils.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "libsockdriver.h"
#include "proto.h"

#define SOCK_HASH_LOG2 7
#define SOCK_HASH_SIZE ((unsigned long)1 << SOCK_HASH_LOG2)
#define SOCK_HASH_MASK (((unsigned long)1 << SOCK_HASH_LOG2) - 1)

static const char* lib_name = "sockdriver";

static struct list_head sock_hash_table[SOCK_HASH_SIZE];

static inline int sock_gethash(sockid_t id) { return id & SOCK_HASH_MASK; }

static void sock_addhash(struct sock* sock)
{
    unsigned int hash = sock_gethash(sock->id);

    list_add(&sock->hash, &sock_hash_table[hash]);
}

static void sock_unhash(struct sock* sock) { list_del(&sock->hash); }

struct sock* sock_get(sockid_t id)
{
    unsigned int hash = sock_gethash(id);
    struct sock* sock;

    list_for_each_entry(sock, &sock_hash_table[hash], hash)
    {
        if (sock->id == id) return sock;
    }
    return NULL;
}

static void sockdriver_reset(struct sock* sock, sockid_t id, int domain,
                             int type, int protocol,
                             const struct sockdriver_ops* ops)
{
    memset(sock, 0, sizeof(*sock));

    sock->id = id;
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;

    sock->sndlowat = 1;
    sock->rcvlowat = 1;

    sock->peercred.pid = -1;
    sock->peercred.uid = -1;
    sock->peercred.gid = -1;

    sock->sel_endpoint = NO_TASK;
    sock->sel_mask = 0;
    sock->sel_oneshot_mask = 0;

    sock->ops = ops;

    INIT_LIST_HEAD(&sock->wq);
    skb_queue_init(&sock->recvq);

    sock_addhash(sock);
}

void sockdriver_clone(struct sock* sock, struct sock* newsock, sockid_t newid)
{
    sockdriver_reset(newsock, newid, sock->domain, sock->type, sock->protocol,
                     sock->ops);

    newsock->flags = sock->flags & ~SFL_ACCEPTCONN;
    newsock->sndlowat = sock->sndlowat;
    newsock->rcvlowat = sock->rcvlowat;
    newsock->linger = sock->linger;
}

void sockdriver_set_shutdown(struct sock* sock, unsigned int flags)
{
    unsigned int mask;

    flags &= ~(unsigned int)sock->flags;

    if (flags != 0) {
        sock->flags |= flags;

        mask = 0;
        if (flags & SFL_SHUT_RD) mask |= SEV_RECV;
        if (flags & SFL_SHUT_WR) mask |= SEV_SEND;
        if (sock_is_listening(sock)) mask |= SEV_ACCEPT;

        assert(mask);
        sockdriver_fire(sock, mask);
    }
}

void sock_free(struct sock* sock)
{
    const struct sockdriver_ops* ops;

    sock_unhash(sock);

    ops = sock->ops;
    sock->ops = NULL;

    ops->sop_free(sock);
}

static int socket_create(const struct sockdriver* sd, endpoint_t endpoint,
                         int domain, int type, int protocol, struct sock** spp)
{
    sockid_t sock_id;
    struct sock* sock;
    const struct sockdriver_ops* ops;

    if (domain <= 0 || domain >= PF_MAX) return EAFNOSUPPORT;

    if ((sock_id =
             sd->sd_create(endpoint, domain, type, protocol, &sock, &ops)) < 0)
        return -sock_id;

    sockdriver_reset(sock, sock_id, domain, type, protocol, ops);
    *spp = sock;

    return 0;
}

sockid_t sockdriver_create_socket(const struct sockdriver* sd,
                                  endpoint_t endpoint, int domain, int type,
                                  int protocol)
{
    struct sock* sock;
    int retval;

    if ((retval = socket_create(sd, endpoint, domain, type, protocol, &sock)) !=
        0)
        return -retval;

    return sock->id;
}

static void send_generic_reply(endpoint_t src, int req_id, int status)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SDEV_REPLY;

    msg.u.m_sockdriver_reply.status = status;
    msg.u.m_sockdriver_reply.req_id = req_id;

    send_recv(SEND, src, &msg);
}

static void send_socket_reply(endpoint_t src, int req_id, int sock_id,
                              int sock_id2)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SDEV_SOCKET_REPLY;

    msg.u.m_sockdriver_socket_reply.req_id = req_id;

    if (sock_id < 0)
        msg.u.m_sockdriver_socket_reply.status = -sock_id;
    else {
        msg.u.m_sockdriver_socket_reply.sock_id = sock_id;
        msg.u.m_sockdriver_socket_reply.sock_id2 = sock_id2;
    }

    send_recv(SEND, src, &msg);
}

static void send_accept_reply(endpoint_t src, int req_id, int status,
                              mgrant_id_t grant, size_t grant_len,
                              const struct sockaddr* addr, socklen_t addrlen)
{
    sockid_t id;
    MESSAGE msg;

    if (status >= 0) {
        id = status;
        status = 0;
    } else {
        id = -1;
        status = -status;
    }

    if (status == OK && grant != GRANT_INVALID) {
        if (addrlen > grant_len) addrlen = grant_len;

        if (addrlen > 0)
            status = safecopy_to(src, grant, 0, (void*)addr, addrlen);
    } else
        addrlen = 0;

    memset(&msg, 0, sizeof(msg));
    msg.type = SDEV_ACCEPT_REPLY;
    msg.u.m_sockdriver_accept_reply.req_id = req_id;
    msg.u.m_sockdriver_accept_reply.status = status;
    msg.u.m_sockdriver_accept_reply.sock_id = id;
    msg.u.m_sockdriver_accept_reply.len = addrlen;

    send_recv(SEND, src, &msg);
}

static void send_recv_reply(endpoint_t src, int req_id, int status,
                            socklen_t ctl_len, mgrant_id_t addr_grant,
                            size_t grant_len, const struct sockaddr* addr,
                            socklen_t addr_len)
{
    MESSAGE msg;
    int retval;

    memset(&msg, 0, sizeof(msg));
    msg.type = SDEV_RECV_REPLY;

    if (status >= 0 && addr_len > 0 && addr_grant != GRANT_INVALID) {
        if (addr_len > grant_len) addr_len = grant_len;

        if ((retval = safecopy_to(src, addr_grant, 0, addr, addr_len)) != OK) {
            status = -retval;
        }
    } else
        addr_len = 0;

    msg.u.m_sockdriver_recv_reply.req_id = req_id;
    msg.u.m_sockdriver_recv_reply.status = status;
    msg.u.m_sockdriver_recv_reply.ctl_len = ctl_len;
    msg.u.m_sockdriver_recv_reply.addr_len = addr_len;

    send_recv(SEND, src, &msg);
}

static void send_poll_notify(endpoint_t src, sockid_t id, __poll_t ops)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = SDEV_POLL_NOTIFY;

    msg.u.m_sockdriver_poll_notify.sock_id = id;
    msg.u.m_sockdriver_poll_notify.ops = ops;

    send_recv(SEND, src, &msg);
}

static void do_socket(const struct sockdriver* sd, MESSAGE* msg)
{
    sockid_t sock_id;

    sock_id = sockdriver_create_socket(sd, msg->u.m_sockdriver_socket.endpoint,
                                       msg->u.m_sockdriver_socket.domain,
                                       msg->u.m_sockdriver_socket.type,
                                       msg->u.m_sockdriver_socket.protocol);

    send_socket_reply(msg->source, msg->u.m_sockdriver_socket.req_id, sock_id,
                      0);
}

static void do_socketpair(const struct sockdriver* sd, MESSAGE* msg)
{
    endpoint_t endpoint = msg->u.m_sockdriver_socket.endpoint;
    int domain = msg->u.m_sockdriver_socket.domain;
    int type = msg->u.m_sockdriver_socket.type;
    int protocol = msg->u.m_sockdriver_socket.protocol;
    struct sock *sock1, *sock2;
    sockid_t sock_id, sock_id2 = -1;
    int retval;

    if ((retval = socket_create(sd, endpoint, domain, type, protocol,
                                &sock1)) != 0) {
        sock_id = -retval;
        goto reply;
    }

    if (sock1->ops->sop_pair == NULL) {
        sock_free(sock1);
        sock_id = -EOPNOTSUPP;
        goto reply;
    }

    if ((retval = socket_create(sd, endpoint, domain, type, protocol,
                                &sock2)) != 0) {
        sock_free(sock1);
        sock_id = -retval;
        goto reply;
    }

    if ((retval = sock1->ops->sop_pair(sock1, sock2, endpoint)) != OK) {
        sock_free(sock1);
        sock_free(sock2);
        sock_id = -retval;
        goto reply;
    }

    sock_id = sock1->id;
    sock_id2 = sock2->id;

reply:
    send_socket_reply(msg->source, msg->u.m_sockdriver_socket.req_id, sock_id,
                      sock_id2);
}

static int do_bind(sockid_t id, struct sockaddr* addr, size_t addrlen,
                   endpoint_t user_endpoint, int flags)
{
    struct sock* sock;

    if ((sock = sock_get(id)) == NULL) return EINVAL;
    if (!sock->ops->sop_bind) return EOPNOTSUPP;
    if (sock->flags & SFL_ACCEPTCONN) return EINVAL;

    return sock->ops->sop_bind(sock, addr, addrlen, user_endpoint, flags);
}

static int do_connect(sockid_t id, struct sockaddr* addr, size_t addrlen,
                      endpoint_t user_endpoint, int flags)
{
    struct sock* sock;
    int retval;

    if ((sock = sock_get(id)) == NULL) return EINVAL;
    if (!sock->ops->sop_connect) return EOPNOTSUPP;
    if (sock->flags & SFL_ACCEPTCONN) return EINVAL;

    retval = sock->ops->sop_connect(sock, addr, addrlen, user_endpoint, flags);

    if (retval == OK) sockdriver_fire(sock, SEV_SEND);

    return retval;
}

static void do_bindconn(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_bindconn.req_id;
    sockid_t sockid = msg->u.m_sockdriver_bindconn.sock_id;
    mgrant_id_t grant = msg->u.m_sockdriver_bindconn.grant;
    size_t len = msg->u.m_sockdriver_bindconn.len;
    endpoint_t user_endpt = msg->u.m_sockdriver_bindconn.user_endpoint;
    int flags = msg->u.m_sockdriver_bindconn.flags;
    char buf[SOCKADDR_MAX];
    int retval;

    if (grant == GRANT_INVALID || len == 0 || len > sizeof(buf)) {
        retval = EINVAL;
        goto reply;
    }

    if ((retval = safecopy_from(src, grant, 0, buf, len)) != OK) goto reply;

    switch (msg->type) {
    case SDEV_BIND:
        retval = do_bind(sockid, (struct sockaddr*)buf, len, user_endpt, flags);
        break;
    case SDEV_CONNECT:
        retval =
            do_connect(sockid, (struct sockaddr*)buf, len, user_endpt, flags);
        break;
    default:
        panic("%s: do_bindconn() unexpected message type", lib_name);
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_listen(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_simple.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_simple.sock_id;
    int backlog = msg->u.m_sockdriver_simple.param;
    int retval;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = EINVAL;
        goto reply;
    }
    if (!sock->ops->sop_listen) {
        retval = EOPNOTSUPP;
        goto reply;
    }

    if (backlog < 0) backlog = 0;
    if (backlog > SOMAXCONN) backlog = SOMAXCONN;

    retval = sock->ops->sop_listen(sock, backlog);

    if (retval == OK) {
        sock->flags |= SFL_ACCEPTCONN;
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_accept(MESSAGE* msg)
{
    struct sock *sock, *newsock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_bindconn.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_bindconn.sock_id;
    mgrant_id_t grant = msg->u.m_sockdriver_bindconn.grant;
    size_t grant_len = msg->u.m_sockdriver_bindconn.len;
    socklen_t len;
    endpoint_t user_endpt = msg->u.m_sockdriver_bindconn.user_endpoint;
    int flags = msg->u.m_sockdriver_bindconn.flags;
    char buf[SOCKADDR_MAX];
    struct sockaddr* addr;
    sockid_t retval;

    len = 0;
    newsock = NULL;
    addr = (struct sockaddr*)buf;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = -EINVAL;
        goto reply;
    }
    if (!sock->ops->sop_accept) {
        retval = -EOPNOTSUPP;
        goto reply;
    }

    retval =
        sock->ops->sop_accept(sock, addr, &len, user_endpt, flags, &newsock);

reply:
    send_accept_reply(src, req_id, retval, grant, grant_len, addr, len);
}

static void sockdriver_sigpipe(struct sock* sock, endpoint_t user_endpt,
                               int flags)
{
    if (sock->type != SOCK_STREAM) return;
    if (flags & MSG_NOSIGNAL) return;

    kernel_kill(user_endpt, SIGPIPE);
}

static int sockdriver_has_suspended(struct sock* sock, int mask)
{
    struct worker_thread* wp;
    list_for_each_entry(wp, &sock->wq, list)
    {
        if (wp->events & mask) return TRUE;
    }
    return FALSE;
}

static ssize_t sockdriver_send(sockid_t id, struct iov_grant_iter* data_iter,
                               size_t data_len,
                               const struct sockdriver_data* ctl_data,
                               size_t ctl_len, struct sockaddr* addr,
                               socklen_t addr_len, endpoint_t user_endpt,
                               int flags)
{
    struct sock* sock;
    ssize_t retval;

    if ((sock = sock_get(id)) == NULL) return -EINVAL;

    if ((retval = sock->err) != OK) {
        sock->err = OK;
        return -retval;
    }

    if (sock->flags & SFL_SHUT_WR) {
        sockdriver_sigpipe(sock, user_endpt, flags);
        return -EPIPE;
    }

    if (sock->flags & SFL_LOCALROUTE) flags |= MSG_DONTROUTE;

    if (sock->ops->sop_send_check &&
        (retval =
             sock->ops->sop_send_check(sock, data_len, ctl_len, addr, addr_len,
                                       user_endpt, flags & ~MSG_DONTWAIT)) != 0)
        return -retval;

    if (!sock->ops->sop_send) return -EOPNOTSUPP;

    do {
        if (!sockdriver_has_suspended(sock, SEV_SEND)) {
            /* try if there is no suspended sender */
            retval =
                sock->ops->sop_send(sock, data_iter, data_len, ctl_data,
                                    ctl_len, addr, addr_len, user_endpt, flags);
            break;
        }

        if (flags & MSG_DONTWAIT) {
            retval = -EAGAIN;
            break;
        }

        sockdriver_suspend(sock, SEV_SEND);

        /* check again */
        if ((retval = sock->err) != OK) {
            sock->err = OK;
            return -retval;
        }

        if (sock->flags & SFL_SHUT_WR) {
            sockdriver_sigpipe(sock, user_endpt, flags);
            return -EPIPE;
        }
    } while (TRUE);

    if (retval == -EPIPE) sockdriver_sigpipe(sock, user_endpt, flags);
    return retval;
}

static void do_send(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_sendrecv.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_sendrecv.sock_id;
    mgrant_id_t data_grant = msg->u.m_sockdriver_sendrecv.data_grant;
    size_t data_len = msg->u.m_sockdriver_sendrecv.data_len;
    mgrant_id_t addr_grant = msg->u.m_sockdriver_sendrecv.addr_grant;
    size_t addr_len = msg->u.m_sockdriver_sendrecv.addr_len;
    endpoint_t user_endpt = msg->u.m_sockdriver_sendrecv.user_endpoint;
    int flags = msg->u.m_sockdriver_sendrecv.flags;
    struct iovec_grant data_iov;
    struct iov_grant_iter data_iter;
    char buf[SOCKADDR_MAX];
    struct sockaddr* addr;
    ssize_t retval = OK;

    data_iov.iov_grant = data_grant;
    data_iov.iov_len = data_len;
    iov_grant_iter_init(&data_iter, src, &data_iov, 1, data_len);

    if (addr_grant != GRANT_INVALID) {
        if (addr_len == 0 || addr_len > sizeof(buf)) {
            retval = -EINVAL;
            goto reply;
        }

        retval = safecopy_from(src, addr_grant, 0, buf, addr_len);
        addr = (struct sockaddr*)buf;
    } else {
        addr = NULL;
        addr_len = 0;
    }

    if (retval != OK) goto reply;

    retval = sockdriver_send(sock_id, &data_iter, data_len, NULL, 0, addr,
                             addr_len, user_endpt, flags);

reply:
    send_generic_reply(src, req_id, retval);
}

static ssize_t sockdriver_recv(sockid_t id, struct iov_grant_iter* data_iter,
                               size_t data_len,
                               const struct sockdriver_data* ctl_data,
                               socklen_t* ctl_len, struct sockaddr* addr,
                               socklen_t* addr_len, endpoint_t user_endpt,
                               int* flags)
{
    struct sock* sock;
    int inflags;
    int oob;
    int check_error = FALSE;
    ssize_t retval;

    inflags = *flags;

    if ((sock = sock_get(id)) == NULL) return -EINVAL;

    if (sock->ops->sop_recv_check &&
        (retval = sock->ops->sop_recv_check(sock, user_endpt,
                                            inflags & ~MSG_DONTWAIT)) != 0)
        return -retval;

    if (sock->flags & SFL_SHUT_RD) return 0;

    if (!sock->ops->sop_recv) return -EOPNOTSUPP;

    oob = inflags & MSG_OOB;
    if (oob && (sock->flags & SFL_URGINLINE)) return -EINVAL;

    do {
        if (oob || !sockdriver_has_suspended(sock, SEV_RECV)) {
            /* try if there is no suspended receiver */
            retval = sock->ops->sop_recv(sock, data_iter, data_len, ctl_data,
                                         ctl_len, addr, addr_len, user_endpt,
                                         inflags, flags);
            break;
        }

        if (inflags & MSG_DONTWAIT) {
            retval = -EAGAIN;
            break;
        }

        sockdriver_suspend(sock, SEV_RECV);

        /* check again */
        if (sock->flags & SFL_SHUT_RD) return 0;

        if (check_error && sock->err != OK) return -sock->err;

        check_error = TRUE;
    } while (TRUE);

    return retval;
}

static void do_recv(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_sendrecv.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_sendrecv.sock_id;
    mgrant_id_t data_grant = msg->u.m_sockdriver_sendrecv.data_grant;
    size_t data_len = msg->u.m_sockdriver_sendrecv.data_len;
    mgrant_id_t addr_grant = msg->u.m_sockdriver_sendrecv.addr_grant;
    size_t addr_grant_len = msg->u.m_sockdriver_sendrecv.addr_len;
    endpoint_t user_endpt = msg->u.m_sockdriver_sendrecv.user_endpoint;
    int flags = msg->u.m_sockdriver_sendrecv.flags;
    struct iovec_grant iov;
    struct iov_grant_iter data_iter;
    char buf[SOCKADDR_MAX];
    struct sockaddr* addr;
    socklen_t addr_len;
    ssize_t retval = OK;

    iov.iov_grant = data_grant;
    iov.iov_len = data_len;
    iov_grant_iter_init(&data_iter, src, &iov, 1, data_len);

    addr = (struct sockaddr*)buf;
    addr_len = 0;

    retval = sockdriver_recv(sock_id, &data_iter, data_len, NULL, NULL, addr,
                             &addr_len, user_endpt, &flags);

    send_recv_reply(src, req_id, retval, 0, addr_grant, addr_grant_len, addr,
                    addr_len);
}

static void do_vsend(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_sendrecv.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_sendrecv.sock_id;
    mgrant_id_t data_grant = msg->u.m_sockdriver_sendrecv.data_grant;
    size_t iov_len = msg->u.m_sockdriver_sendrecv.data_len;
    mgrant_id_t ctl_grant = msg->u.m_sockdriver_sendrecv.ctl_grant;
    size_t ctl_len = msg->u.m_sockdriver_sendrecv.ctl_len;
    mgrant_id_t addr_grant = msg->u.m_sockdriver_sendrecv.addr_grant;
    size_t addr_len = msg->u.m_sockdriver_sendrecv.addr_len;
    endpoint_t user_endpt = msg->u.m_sockdriver_sendrecv.user_endpoint;
    int flags = msg->u.m_sockdriver_sendrecv.flags;
    struct iovec_grant data_iov[NR_IOREQS];
    struct iov_grant_iter data_iter;
    size_t data_len = 0;
    struct sockdriver_data ctl_data;
    char buf[SOCKADDR_MAX];
    struct sockaddr* addr;
    int i;
    ssize_t retval = OK;

    if (iov_len > 0) {
        if (iov_len > NR_IOREQS) {
            retval = -EINVAL;
            goto reply;
        }

        if ((retval = safecopy_from(src, data_grant, 0, data_iov,
                                    sizeof(data_iov[0]) * iov_len)) != OK) {
            retval = -retval;
            goto reply;
        }

        for (i = 0; i < iov_len; i++)
            data_len += data_iov[i].iov_len;
    }

    iov_grant_iter_init(&data_iter, src, data_iov, iov_len, data_len);

    ctl_data.endpoint = src;
    ctl_data.grant = ctl_grant;
    ctl_data.len = ctl_len;

    if (addr_grant != GRANT_INVALID) {
        if (addr_len == 0 || addr_len > sizeof(buf)) {
            retval = -EINVAL;
            goto reply;
        }

        retval = safecopy_from(src, addr_grant, 0, buf, addr_len);
        addr = (struct sockaddr*)buf;
    } else {
        addr = NULL;
        addr_len = 0;
    }

    if (retval != OK) goto reply;

    retval = sockdriver_send(sock_id, &data_iter, data_len, &ctl_data, ctl_len,
                             addr, addr_len, user_endpt, flags);

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_vrecv(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_sendrecv.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_sendrecv.sock_id;
    mgrant_id_t data_grant = msg->u.m_sockdriver_sendrecv.data_grant;
    size_t iov_len = msg->u.m_sockdriver_sendrecv.data_len;
    mgrant_id_t ctl_grant = msg->u.m_sockdriver_sendrecv.ctl_grant;
    socklen_t ctl_len = msg->u.m_sockdriver_sendrecv.ctl_len;
    mgrant_id_t addr_grant = msg->u.m_sockdriver_sendrecv.addr_grant;
    size_t addr_grant_len = msg->u.m_sockdriver_sendrecv.addr_len;
    endpoint_t user_endpt = msg->u.m_sockdriver_sendrecv.user_endpoint;
    int flags = msg->u.m_sockdriver_sendrecv.flags;
    struct iovec_grant data_iov[NR_IOREQS];
    struct iov_grant_iter data_iter;
    struct sockdriver_data ctl_data;
    char buf[SOCKADDR_MAX];
    struct sockaddr* addr;
    socklen_t addr_len;
    size_t data_len = 0;
    int i;
    ssize_t retval = OK;

    if (iov_len > 0) {
        if (iov_len > NR_IOREQS) {
            retval = -EINVAL;
            goto reply;
        }

        if ((retval = safecopy_from(src, data_grant, 0, data_iov,
                                    sizeof(data_iov[0]) * iov_len)) != OK) {
            retval = -retval;
            goto reply;
        }

        for (i = 0; i < iov_len; i++)
            data_len += data_iov[i].iov_len;
    }

    iov_grant_iter_init(&data_iter, src, data_iov, iov_len, data_len);

    ctl_data.endpoint = src;
    ctl_data.grant = ctl_grant;
    ctl_data.len = ctl_len;

    addr = (struct sockaddr*)buf;
    addr_len = 0;

    retval = sockdriver_recv(sock_id, &data_iter, data_len, &ctl_data, &ctl_len,
                             addr, &addr_len, user_endpt, &flags);

reply:
    send_recv_reply(src, req_id, retval, ctl_len, addr_grant, addr_grant_len,
                    addr, addr_len);
}

static void do_select(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_select.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_select.sock_id;
    __poll_t ops = msg->u.m_sockdriver_select.ops;
    int notify, oneshot;
    __poll_t retval;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = -EINVAL;
        goto reply;
    }

    notify = ops & POLL_NOTIFY;
    oneshot = ops & POLL_ONESHOT;
    ops &= ~(POLL_NOTIFY | POLL_ONESHOT);

    retval = sock->ops->sop_poll(sock) & ops;

    if (notify && ops) {
        if (oneshot) {
            ops &= ~retval;
            /* set outstanding oneshot events */
            sock->sel_oneshot_mask |= ops & ~sock->sel_mask;
        }

        if (sock->sel_endpoint != NO_TASK) {
            if (sock->sel_endpoint != src) {
                retval = -EIO;
                goto reply;
            }

            sock->sel_mask |= ops;
        } else {
            sock->sel_endpoint = src;
            sock->sel_mask = ops;
        }
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_ioctl(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_ioctl.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_ioctl.sock_id;
    unsigned long request = msg->u.m_sockdriver_ioctl.request;
    endpoint_t user_endpt = msg->u.m_sockdriver_ioctl.endpoint;
    int flags = msg->u.m_sockdriver_ioctl.flags;
    struct sockdriver_data data;
    int retval;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = EINVAL;
        goto reply;
    }

    data.endpoint = src;
    data.grant = msg->u.m_sockdriver_ioctl.grant;
    data.len = _IOC_SIZE(request);

    if (!sock->ops->sop_ioctl) {
        retval = ENOTTY;
        goto reply;
    }

    retval = sock->ops->sop_ioctl(sock, request, &data, user_endpt, flags);

reply:
    send_generic_reply(src, req_id, retval);
}

static inline int sockopt_to_flag(int name)
{
    switch (name) {
    case SO_DEBUG:
        return SFL_DBG;
    case SO_ACCEPTCONN:
        return SFL_ACCEPTCONN;
    case SO_REUSEADDR:
        return SFL_REUSE;
    case SO_KEEPALIVE:
        return SFL_KEEPOPEN;
    case SO_DONTROUTE:
        return SFL_LOCALROUTE;
    case SO_BROADCAST:
        return SFL_BROADCAST;
    case SO_OOBINLINE:
        return SFL_URGINLINE;
    case SO_PASSCRED:
        return SFL_PASSCRED;
    default:
        return 0;
    }
}

static void do_getsockopt(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_getset.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_getset.sock_id;
    int level = msg->u.m_sockdriver_getset.level;
    int name = msg->u.m_sockdriver_getset.name;
    mgrant_id_t grant = msg->u.m_sockdriver_getset.grant;
    socklen_t len = msg->u.m_sockdriver_getset.len;
    struct sockdriver_data data;
    int val, flag;
    int retval = 0;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = -EINVAL;
        goto reply;
    }

    data.endpoint = src;
    data.grant = grant;
    data.len = len;

    if (level == SOL_SOCKET) {
        switch (name) {
        case SO_DEBUG:
        case SO_ACCEPTCONN:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_OOBINLINE:
        case SO_PASSCRED:
            flag = sockopt_to_flag(name);
            assert(flag);

            val = !!(sock->flags & flag);
            len = sizeof(val);
            retval = sockdriver_copyout(&data, 0, &val, len);
            break;
        case SO_ERROR:
            val = sock_error(sock);
            len = sizeof(val);
            retval = sockdriver_copyout(&data, 0, &val, len);
            break;
        case SO_TYPE:
            val = sock_type(sock);
            len = sizeof(val);
            retval = sockdriver_copyout(&data, 0, &val, len);
            break;
        case SO_PEERCRED: {
            struct ucred peercred;
            if (len > sizeof(peercred)) len = sizeof(peercred);
            peercred.pid = sock_peercred(sock).pid;
            peercred.uid = sock_peercred(sock).uid;
            peercred.gid = sock_peercred(sock).gid;
            retval = sockdriver_copyout(&data, 0, &peercred, len);
            break;
        }
        default:
            break;
        }
    } else {
        if (sock->ops->sop_getsockopt == NULL)
            retval = ENOPROTOOPT;
        else
            retval = sock->ops->sop_getsockopt(sock, level, name, &data, &len);
    }

    if (retval == OK)
        retval = len;
    else
        retval = -retval;

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_setsockopt(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_getset.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_getset.sock_id;
    int level = msg->u.m_sockdriver_getset.level;
    int name = msg->u.m_sockdriver_getset.name;
    mgrant_id_t grant = msg->u.m_sockdriver_getset.grant;
    socklen_t len = msg->u.m_sockdriver_getset.len;
    struct sockdriver_data data;
    int val, flag;
    int retval = 0;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = EINVAL;
        goto reply;
    }

    data.endpoint = src;
    data.grant = grant;
    data.len = len;

    if (level == SOL_SOCKET) {
        switch (name) {
        case SO_DEBUG:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_OOBINLINE:
        case SO_PASSCRED:
            if ((retval = sockdriver_copyin(&data, 0, &val, len)) != OK)
                goto reply;

            flag = sockopt_to_flag(name);
            assert(flag);

            if (val)
                sock->flags |= flag;
            else
                sock->flags &= ~flag;
            break;
        /* case SO_SNDBUF: */
        /*     if ((retval = sockdriver_copyin(&data, 0, &val, len)) != OK) */
        /*         goto reply; */
        /*     val = min(val, INT_MAX / 2); */
        /*     sock->sndbuf = val * 2; */
        /*     sockdriver_fire(sock, SEV_SEND); */
        /*     break; */
        case SO_ACCEPTCONN:
        case SO_ERROR:
        case SO_TYPE:
        case SO_PEERCRED: {
            retval = ENOPROTOOPT;
            break;
        }
        default:
            break;
        }
    } else {
        if (sock->ops->sop_setsockopt == NULL)
            retval = ENOPROTOOPT;
        else
            retval = sock->ops->sop_setsockopt(sock, level, name, &data, len);
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_getname(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_getset.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_getset.sock_id;
    mgrant_id_t grant = msg->u.m_sockdriver_getset.grant;
    socklen_t len, addr_len = msg->u.m_sockdriver_getset.len;
    char buf[SOCKADDR_MAX];
    int retval = 0;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = -EINVAL;
        goto reply;
    }

    switch (msg->type) {
    case SDEV_GETSOCKNAME:
        if (!sock->ops->sop_getsockname)
            retval = -EOPNOTSUPP;
        else
            retval =
                sock->ops->sop_getsockname(sock, (struct sockaddr*)buf, &len);
        break;
    default:
        panic("%s: %s unknown request", lib_name, __FUNCTION__);
        break;
    }

    if (retval == OK) {
        if (len == 0)
            panic("%s: %s success but no address", lib_name, __FUNCTION__);

        if (addr_len > len) addr_len = len;

        if (addr_len > 0 &&
            (retval = safecopy_to(src, grant, 0, buf, addr_len)) == 0)
            retval = addr_len;
    } else if (retval > 0)
        panic("%s: %s invalid reply status %d", lib_name, __FUNCTION__, retval);

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_close(MESSAGE* msg)
{
    struct sock* sock;
    endpoint_t src = msg->source;
    int req_id = msg->u.m_sockdriver_simple.req_id;
    sockid_t sock_id = msg->u.m_sockdriver_simple.sock_id;
    int flags = msg->u.m_sockdriver_simple.param;
    int force, retval;

    if ((sock = sock_get(sock_id)) == NULL) {
        retval = EINVAL;
        goto reply;
    }

    sock->sel_endpoint = NO_TASK;

    force = ((sock->flags & SFL_LINGER) && sock->linger == 0);

    if (sock->ops->sop_close)
        retval = sock->ops->sop_close(sock, force);
    else
        retval = 0;

    assert(retval == 0 || retval == SUSPEND);

    if (retval == SUSPEND) {
        sock->flags |= SFL_CLOSING;

        retval = 0;

        if (force) goto reply;

        if (!(flags & SDEV_NONBLOCK)) {
            sockdriver_suspend(sock, SEV_CLOSE);
            sock_free(sock);
        }
    } else
        sock_free(sock);

reply:
    send_generic_reply(src, req_id, retval);
}

void sockdriver_suspend(struct sock* sock, unsigned int event)
{
    struct worker_thread* wp = sockdriver_worker();

    wp->events = event;
    list_add(&wp->list, &sock->wq);

    sockdriver_sleep();
}

void sockdriver_fire(struct sock* sock, unsigned int mask)
{
    struct worker_thread *wp, *tmp;
    __poll_t ops, retval;

    if (mask & SEV_CONNECT) mask |= SEV_SEND;

    if ((mask & SEV_CLOSE) && list_empty(&sock->wq)) {
        assert(mask == SEV_CLOSE);
        assert(sock_is_closing(sock));
        sock_free(sock);
        return;
    }

    list_for_each_entry_safe(wp, tmp, &sock->wq, list)
    {
        if (wp->events & mask) {
            list_del(&wp->list);
            sockdriver_wakeup_worker(wp);
        }
    }

    if (sock->sel_endpoint != NO_TASK) {
        ops = sock->sel_mask;

        retval = sock->ops->sop_poll(sock) & ops;

        if (retval != 0) {
            send_poll_notify(sock->sel_endpoint, sock_sockid(sock), retval);

            if (sock->sel_oneshot_mask) {
                /* clear oneshot events */
                sock->sel_mask &= ~(retval & sock->sel_oneshot_mask);

                if (sock->sel_mask == 0) sock->sel_endpoint = NO_TASK;
            }
        }
    }
}

void sockdriver_init(void)
{
    int i;

    for (i = 0; i < SOCK_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&sock_hash_table[i]);
    }
}

void sockdriver_process(const struct sockdriver* sd, MESSAGE* msg)
{
    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case CLOCK:
            if (sd->sd_alarm) sd->sd_alarm(msg->TIMESTAMP);
            break;
        default:
            if (sd->sd_other) sd->sd_other(msg);
            break;
        }
    }

    switch (msg->type) {
    case SDEV_SOCKET:
        do_socket(sd, msg);
        break;
    case SDEV_SOCKETPAIR:
        do_socketpair(sd, msg);
        break;
    case SDEV_BIND:
    case SDEV_CONNECT:
        do_bindconn(msg);
        break;
    case SDEV_LISTEN:
        do_listen(msg);
        break;
    case SDEV_ACCEPT:
        do_accept(msg);
        break;
    case SDEV_SEND:
        do_send(msg);
        break;
    case SDEV_RECV:
        do_recv(msg);
        break;
    case SDEV_VSEND:
        do_vsend(msg);
        break;
    case SDEV_VRECV:
        do_vrecv(msg);
        break;
    case SDEV_SELECT:
        do_select(msg);
        break;
    case SDEV_IOCTL:
        do_ioctl(msg);
        break;
    case SDEV_GETSOCKOPT:
        do_getsockopt(msg);
        break;
    case SDEV_SETSOCKOPT:
        do_setsockopt(msg);
        break;
    case SDEV_GETSOCKNAME:
        do_getname(msg);
        break;
    case SDEV_CLOSE:
        do_close(msg);
        break;
    default:
        if (sd->sd_other) {
            sd->sd_other(msg);
        }
    }
}

int sockdriver_copyin(const struct sockdriver_data* data, size_t off, void* ptr,
                      size_t len)
{
    return safecopy_from(data->endpoint, data->grant, off, ptr, len);
}

int sockdriver_copyout(const struct sockdriver_data* data, size_t off,
                       void* ptr, size_t len)
{
    return safecopy_to(data->endpoint, data->grant, off, ptr, len);
}
