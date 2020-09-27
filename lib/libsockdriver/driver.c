#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>

#include "libsockdriver.h"

#define SOCK_HASH_LOG2 7
#define SOCK_HASH_SIZE ((unsigned long)1 << SOCK_HASH_LOG2)
#define SOCK_HASH_MASK (((unsigned long)1 << SOCK_HASH_LOG2) - 1)

static struct list_head sock_hash_table[SOCK_HASH_SIZE];

static sockdriver_socket_cb_t sockdriver_socket_cb;

static inline int sock_gethash(struct sockdriver_sock* sock)
{
    return sock->id & SOCK_HASH_MASK;
}

static void sock_addhash(struct sockdriver_sock* sock)
{
    unsigned int hash = sock_gethash(sock);

    list_add(&sock->hash, &sock_hash_table[hash]);
}

static void sock_unhash(struct sockdriver_sock* sock) { list_del(&sock->hash); }

static void sockevent_reset(struct sockdriver_sock* sock, sockid_t id,
                            int domain, int type,
                            const struct sockdriver_ops* ops)
{

    memset(sock, 0, sizeof(*sock));

    sock->id = id;
    sock->domain = domain;
    sock->type = type;

    sock->ops = ops;

    sock_addhash(sock);
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

    sockevent_reset(sock, sock_id, domain, type, ops);
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

static void do_socket(MESSAGE* msg)
{
    sockid_t sock_id;

    sock_id = socket_socket(
        msg->u.m_sockdriver_socket.endpoint, msg->u.m_sockdriver_socket.domain,
        msg->u.m_sockdriver_socket.type, msg->u.m_sockdriver_socket.protocol);

    send_socket_reply(msg->source, msg->u.m_sockdriver_socket.req_id, sock_id,
                      0);
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
    }
}
