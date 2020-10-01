#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <lyos/mgrant.h>
#include <lyos/sysutils.h>

#include "libsockdriver.h"
#include "proto.h"

#define SOCK_HASH_LOG2 7
#define SOCK_HASH_SIZE ((unsigned long)1 << SOCK_HASH_LOG2)
#define SOCK_HASH_MASK (((unsigned long)1 << SOCK_HASH_LOG2) - 1)

static const char* name = "sockdriver";

static struct list_head sock_hash_table[SOCK_HASH_SIZE];

static sockdriver_socket_cb_t sockdriver_socket_cb;

static inline int sock_gethash(sockid_t id) { return id & SOCK_HASH_MASK; }

static void sock_addhash(struct sockdriver_sock* sock)
{
    unsigned int hash = sock_gethash(sock->id);

    list_add(&sock->hash, &sock_hash_table[hash]);
}

static void sock_unhash(struct sockdriver_sock* sock) { list_del(&sock->hash); }

static inline struct sockdriver_sock* sock_get(sockid_t id)
{
    unsigned int hash = sock_gethash(id);
    struct sockdriver_sock* sock;

    list_for_each_entry(sock, &sock_hash_table[hash], hash)
    {
        if (sock->id == id) return sock;
    }
    return NULL;
}

static void sockdriver_reset(struct sockdriver_sock* sock, sockid_t id,
                             int domain, int type,
                             const struct sockdriver_ops* ops)
{

    memset(sock, 0, sizeof(*sock));

    sock->id = id;
    sock->domain = domain;
    sock->type = type;
    sock->ops = ops;

    INIT_LIST_HEAD(&sock->wq);

    sock_addhash(sock);
}

void sockdriver_clone(struct sockdriver_sock* sock,
                      struct sockdriver_sock* newsock, sockid_t newid)
{
    sockdriver_reset(newsock, newid, sock->domain, sock->type, sock->ops);

    newsock->sock_opt = sock->sock_opt & ~SO_ACCEPTCONN;
}

static int socket_create(endpoint_t endpoint, int domain, int type,
                         int protocol, struct sockdriver_sock** spp)
{
    sockid_t sock_id;
    struct sockdriver_sock* sock;
    const struct sockdriver_ops* ops;

    if (domain <= 0 || domain >= PF_MAX) return EAFNOSUPPORT;

    if ((sock_id = sockdriver_socket_cb(endpoint, domain, type, protocol, &sock,
                                        &ops)) < 0)
        return -sock_id;

    sockdriver_reset(sock, sock_id, domain, type, ops);
    *spp = sock;

    return 0;
}

static sockid_t socket_socket(endpoint_t endpoint, int domain, int type,
                              int protocol)
{
    struct sockdriver_sock* sock;
    int retval;

    if ((retval = socket_create(endpoint, domain, type, protocol, &sock)) != 0)
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

static void do_socket(MESSAGE* msg)
{
    sockid_t sock_id;

    sock_id = socket_socket(
        msg->u.m_sockdriver_socket.endpoint, msg->u.m_sockdriver_socket.domain,
        msg->u.m_sockdriver_socket.type, msg->u.m_sockdriver_socket.protocol);

    send_socket_reply(msg->source, msg->u.m_sockdriver_socket.req_id, sock_id,
                      0);
}

static int do_bind(sockid_t id, struct sockaddr* addr, size_t addrlen,
                   endpoint_t user_endpoint, int flags)
{
    struct sockdriver_sock* sock;

    if ((sock = sock_get(id)) == NULL) return EINVAL;
    if (!sock->ops->sop_bind) return EOPNOTSUPP;
    if (sock->sock_opt & SO_ACCEPTCONN) return EINVAL;

    return sock->ops->sop_bind(sock, addr, addrlen, user_endpoint, flags);
}

static int do_connect(sockid_t id, struct sockaddr* addr, size_t addrlen,
                      endpoint_t user_endpoint, int flags)
{
    struct sockdriver_sock* sock;
    int retval;

    if ((sock = sock_get(id)) == NULL) return EINVAL;
    if (!sock->ops->sop_connect) return EOPNOTSUPP;
    if (sock->sock_opt & SO_ACCEPTCONN) return EINVAL;

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

    if (grant == GRANT_INVALID || len == 0 || len > buf) {
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
        panic("%s: do_bindconn() unexpected message type");
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_listen(MESSAGE* msg)
{
    struct sockdriver_sock* sock;
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
        sock->sock_opt |= SO_ACCEPTCONN;
    }

reply:
    send_generic_reply(src, req_id, retval);
}

static void do_accept(MESSAGE* msg)
{
    struct sockdriver_sock *sock, *newsock;
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

void sockdriver_suspend(struct sockdriver_sock* sock, unsigned int event)
{
    struct worker_thread* wp = sockdriver_worker();

    wp->events = event;
    list_add(&wp->list, &sock->wq);

    sockdriver_sleep();
}

void sockdriver_fire(struct sockdriver_sock* sock, unsigned int mask)
{
    struct worker_thread *wp, *tmp;

    if (mask & SEV_CONNECT) mask |= SEV_SEND;

    list_for_each_entry_safe(wp, tmp, &sock->wq, list)
    {
        if (wp->events & mask) {
            list_del(&wp->list);
            sockdriver_wakeup_worker(wp);
        }
    }
}

void sockdriver_init(sockdriver_socket_cb_t socket_cb)
{
    int i;

    sockdriver_socket_cb = socket_cb;

    for (i = 0; i < SOCK_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&sock_hash_table[i]);
    }
}

void sockdriver_process(MESSAGE* msg)
{
    switch (msg->type) {
    case SDEV_SOCKET:
        do_socket(msg);
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
    }
}
