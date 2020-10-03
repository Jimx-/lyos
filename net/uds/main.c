#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>

#include "uds.h"

#define MAX_THREADS 128

#define UDS_HASH_LOG2 7
#define UDS_HASH_SIZE ((unsigned long)1 << UDS_HASH_LOG2)
#define UDS_HASH_MASK (((unsigned long)1 << UDS_HASH_LOG2) - 1)

static struct idr sock_idr;

static struct list_head uds_hash_table[UDS_HASH_SIZE];

static sockid_t uds_socket(endpoint_t src, int domain, int type, int protocol,
                           struct sock** sock,
                           const struct sockdriver_ops** ops);

static int uds_bind(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                    endpoint_t user_endpt, int flags);
static int uds_connect(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                       endpoint_t user_endpt, int flags);
static int uds_listen(struct sock* sock, int backlog);
static int uds_accept(struct sock* sock, struct sockaddr* addr,
                      socklen_t* addrlen, endpoint_t user_endpt, int flags,
                      struct sock** newsockp);

static const struct sockdriver uds_driver = {
    .sd_create = uds_socket,
};

static const struct sockdriver_ops uds_ops = {
    .sop_bind = uds_bind,
    .sop_connect = uds_connect,
    .sop_listen = uds_listen,
    .sop_accept = uds_accept,
    .sop_send = uds_send,
    .sop_recv = uds_recv,
    .sop_poll = uds_poll,
};

static inline unsigned int udssock_gethash(dev_t dev, ino_t ino)
{
    return (dev ^ ino) & UDS_HASH_MASK;
}

static void udssock_addhash(struct udssock* uds)
{
    unsigned int hash = udssock_gethash(uds->dev, uds->ino);

    list_add(&uds->hash, &uds_hash_table[hash]);
}

static void udssock_unhash(struct udssock* uds) { list_del(&uds->hash); }

static inline struct udssock* udssock_get(dev_t dev, ino_t ino)
{
    unsigned int hash = udssock_gethash(dev, ino);
    struct udssock* uds;

    list_for_each_entry(uds, &uds_hash_table[hash], hash)
    {
        if (uds->dev == dev && uds->ino == ino) return uds;
    }
    return NULL;
}

static void uds_clear_cred(struct udssock* uds)
{
    uds->cred.pid = -1;
    uds->cred.uid = -1;
    uds->cred.gid = -1;
}

static int uds_alloc(struct udssock** udsp)
{
    struct udssock* uds;
    int retval;

    uds = malloc(sizeof(*uds));
    if (!uds) return ENOMEM;

    if ((retval = uds_io_alloc(uds)) != OK) {
        free(uds);
        return retval;
    }

    memset(uds, 0, sizeof(*uds));
    uds->conn = NULL;
    uds_clear_cred(uds);

    INIT_LIST_HEAD(&uds->queue);

    *udsp = uds;
    return 0;
}

static void uds_free(struct udssock* uds)
{
    uds_io_free(uds);

    free(uds);
}

static void uds_get_cred(struct udssock* uds, endpoint_t endpoint)
{
    if ((uds->cred.pid = get_epinfo(endpoint, &uds->cred.uid, &uds->cred.gid)) <
        0) {
        uds_clear_cred(uds);
    }
}

static void uds_init_peercred(struct udssock* uds, endpoint_t endpoint)
{
    struct sock_cred* cred = &sock_peercred(&uds->sock);

    if ((cred->pid = get_epinfo(endpoint, &cred->uid, &cred->gid)) < 0) {
        cred->pid = -1;
        cred->uid = -1;
        cred->gid = -1;
    }
}

static void uds_copy_peercred(struct udssock* uds, struct udssock* peer)
{
    struct sock_cred* dest_cred = &sock_peercred(&uds->sock);
    struct sock_cred* src_cred = &sock_peercred(&peer->sock);

    memcpy(dest_cred, src_cred, sizeof(*dest_cred));
}

static int uds_check_addr(const struct sockaddr* addr, size_t addrlen,
                          const char** pathp)
{
    size_t len;
    const char* p;

    if (addrlen < offsetof(struct sockaddr, sa_data)) return -EINVAL;
    if (addr->sa_family != AF_UNIX) return -EAFNOSUPPORT;

    len = addrlen - offsetof(struct sockaddr, sa_data);
    if (len > 0 && (p = memchr(addr->sa_data, '\0', len)) != NULL)
        len = p - addr->sa_data;

    if (len == 0) return -ENOENT;

    if (len >= PATH_MAX) return -EINVAL;

    *pathp = (const char*)addr->sa_data;
    return len;
}

void uds_make_addr(const char* path, size_t path_len, struct sockaddr* addr,
                   socklen_t* addr_len)
{
    addr->sa_family = AF_UNIX;

    if (path_len > 0) {
        memcpy((char*)addr->sa_data, path, path_len);
        ((char*)addr->sa_data)[path_len] = '\0';

        path_len++;
    }

    *addr_len = offsetof(struct sockaddr, sa_data) + path_len;
}

static void uds_enqueue(struct udssock* uds, struct udssock* link)
{
    list_add(&link->list, &uds->queue);
    link->link = uds;
    uds->nr_queued++;
}

static void uds_dequeue(struct udssock* uds, struct udssock* link)
{
    list_del(&link->list);
    link->link = NULL;
    uds->nr_queued--;
}

static void uds_disconnect(struct udssock* uds, int has_link)
{
    struct udssock* conn = uds->conn;

    uds->conn = NULL;
    conn->conn = NULL;

    if (has_link) {
        sockdriver_fire(&conn->sock, SEV_CLOSE);
    } else {
        sockdriver_fire(&conn->sock, SEV_SEND | SEV_RECV);
        uds_clear_cred(conn);
    }
}

static void uds_clear_queue(struct udssock* uds, struct udssock* except)
{
    struct udssock *peer, *tmp;

    list_for_each_entry_safe(peer, tmp, &uds->queue, list)
    {
        if (except == peer) continue;

        uds_dequeue(uds, peer);

        if (uds != peer) sock_set_error(&peer->sock, ECONNRESET);

        if (uds_get_type(peer) != SOCK_DGRAM) {
            if (uds_is_connected(peer))
                uds_disconnect(peer, TRUE);
            else
                uds_clear_cred(peer);
        }
    }
}

static sockid_t uds_socket(endpoint_t src, int domain, int type, int protocol,
                           struct sock** sock,
                           const struct sockdriver_ops** ops)
{
    struct udssock* uds;
    sockid_t id;
    int retval;

    if (domain != PF_UNIX) return -EAFNOSUPPORT;

    switch (type) {
    case SOCK_STREAM:
    case SOCK_SEQPACKET:
    case SOCK_DGRAM:
        break;
    default:
        return -EPROTOTYPE;
    }

    if (protocol != 0) return -EPROTONOSUPPORT;

    if ((retval = uds_alloc(&uds)) != 0) return -retval;

    id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (id < 0) {
        uds_free(uds);
    } else {
        *sock = &uds->sock;
        *ops = &uds_ops;
    }

    return id;
}

static int uds_bind(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                    endpoint_t user_endpt, int flags)
{
    struct udssock *uds = to_udssock(sock), *uds2;
    const char* path;
    size_t path_len;
    dev_t dev;
    ino_t ino;
    int retval;

    if (uds_is_bound(uds)) return EINVAL;

    if ((retval = uds_check_addr(addr, addrlen, &path)) < 0) return -retval;
    path_len = retval;

    if ((retval = socketpath(user_endpt, path, path_len, SKP_CREATE, &dev,
                             &ino)) != OK)
        return retval;

    if ((uds2 = udssock_get(dev, ino))) {
        udssock_unhash(uds2);
        uds2->dev = NO_DEV;
        uds2->ino = 0;
    }

    if (uds_get_type(uds) != SOCK_DGRAM && !uds_is_connecting(uds) &&
        !uds_is_connected(uds))
        uds_init_peercred(uds, user_endpt);

    uds->path_len = path_len;
    memcpy(uds->path, path, path_len);
    uds->dev = dev;
    uds->ino = ino;

    udssock_addhash(uds);
    return 0;
}

static int uds_listen(struct sock* sock, int backlog)
{
    struct udssock* uds = to_udssock(sock);

    if (uds_get_type(uds) == SOCK_DGRAM) return EOPNOTSUPP;
    if (uds_is_connecting(uds) || uds_is_connected(uds)) return EINVAL;
    if (!uds_is_bound(uds)) return EDESTADDRREQ;

    uds->flags &= ~UDSF_CONNECTED;
    uds->backlog = backlog;

    return 0;
}

int uds_lookup(struct udssock* uds, const struct sockaddr* addr, size_t addrlen,
               endpoint_t user_endpt, struct udssock** peerp)
{
    const char* path;
    size_t path_len;
    dev_t dev;
    ino_t ino;
    struct udssock* peer;
    int retval;

    if ((retval = uds_check_addr(addr, addrlen, &path)) < 0) return -retval;
    path_len = retval;

    if ((retval = socketpath(user_endpt, path, path_len, SKP_LOOK_UP, &dev,
                             &ino)) != OK)
        return retval;

    if ((peer = udssock_get(dev, ino)) == NULL) return ECONNREFUSED;
    if (uds_get_type(peer) != uds_get_type(uds)) return EPROTOTYPE;

    *peerp = peer;
    return 0;
}

static int uds_attach(struct udssock* uds, struct udssock* link)
{
    struct udssock* conn;
    sockid_t newid;
    int retval;

    if ((retval = uds_alloc(&conn)) != OK) return retval;

    if ((newid = idr_alloc(&sock_idr, NULL, 1, 0)) < 0) return -newid;

    sockdriver_clone(&uds->sock, &conn->sock, newid);

    link->conn = conn;
    link->flags |= UDSF_CONNECTED;
    uds_copy_peercred(link, uds);

    conn->conn = link;
    conn->flags = uds->flags & (UDSF_PASSCRED | UDSF_CONNWAIT);
    conn->flags |= UDSF_CONNECTED;
    conn->path_len = uds->path_len;
    memcpy(conn->path, uds->path, uds->path_len);
    memcpy(&conn->cred, &uds->cred, sizeof(uds->cred));

    return 0;
}

static int uds_connect(struct sock* sock, struct sockaddr* addr, size_t addrlen,
                       endpoint_t user_endpt, int flags)
{
    struct udssock* uds = to_udssock(sock);
    struct udssock* peer;
    int need_wait = FALSE;
    int check_again = TRUE;
    int retval;

restart:
    retval = 0;

    if (uds_get_type(uds) != SOCK_DGRAM) {
        if (uds_is_listening(uds)) return EOPNOTSUPP;
        if (uds_is_connecting(uds)) return EALREADY;
        if (uds_is_connected(uds)) return EISCONN;
    } else {
        /* disconnect socket with an AF_UNSPEC address */
        if (addrlen >= offsetof(struct sockaddr, sa_data) &&
            addr->sa_family == AF_UNSPEC) {
            if (uds_has_link(uds)) uds_dequeue(uds->link, uds);

            return 0;
        }
    }

    if ((retval = uds_lookup(uds, addr, addrlen, user_endpt, &peer)) != 0)
        return retval;

    if (uds_get_type(uds) == SOCK_DGRAM) {
        if (uds_has_link(uds) && uds->link == peer) return 0;

        if (uds != peer && uds_has_link(peer) && peer->link != uds)
            return EPERM;

        if (uds_has_link(uds)) uds_dequeue(uds->link, uds);
        uds_clear_queue(uds, (uds != peer) ? peer : NULL);

        uds_enqueue(peer, uds);
        return 0;
    }

    if (!uds_is_listening(peer)) return ECONNREFUSED;
    if (uds_is_shutdown(peer, SFL_SHUT_RD | SFL_SHUT_WR)) return ECONNREFUSED;
    if (peer->nr_queued >= peer->backlog) return ECONNREFUSED;

    if (!((uds->flags | peer->flags) & UDSF_CONNWAIT)) {
        if ((retval = uds_attach(peer, uds)) != OK) return retval;

        uds_init_peercred(uds->conn, user_endpt);
    } else {
        uds->flags &= ~UDSF_CONNECTED;
        need_wait = TRUE;
    }

    uds_get_cred(uds, user_endpt);
    uds_enqueue(peer, uds);

    sockdriver_fire(&peer->sock, SEV_ACCEPT);

    if (need_wait) {
        if (flags & SDEV_NONBLOCK) {
            retval = EAGAIN;

            if (check_again) {
                /* Yield to (possibly) other workers that are blocked on
                 * accept() on the peer socket to see if they can  accept us
                 * before we report EAGAIN to non-blocking requests. If there is
                 * none, we will be scheduled again right away and fail with
                 * EAGAIN.
                 */
                sockdriver_yield();
                check_again = FALSE;
                goto restart;
            }
        } else {
            sockdriver_suspend(&uds->sock, SEV_CONNECT);
            if ((retval = sock_error(&uds->sock)) == OK) goto restart;
        }
    }

    return retval;
}

static int uds_accept(struct sock* sock, struct sockaddr* addr,
                      socklen_t* addrlen, endpoint_t user_endpt, int flags,
                      struct sock** newsockp)
{
    struct udssock* uds = to_udssock(sock);
    struct udssock *peer, *conn;
    sockid_t retval;

restart:
    if (uds_get_type(uds) == SOCK_DGRAM) return -EOPNOTSUPP;
    if (!uds_is_listening(uds)) return -EINVAL;

    if (list_empty(&uds->queue)) {
        if (uds_is_shutdown(uds, SFL_SHUT_RD | SFL_SHUT_WR))
            return -ECONNABORTED;

        if (flags & SDEV_NONBLOCK) return -EAGAIN;

        sockdriver_suspend(sock, SEV_ACCEPT);
        goto restart;
    }

    peer = list_first_entry(&uds->queue, struct udssock, list);

    if (uds_is_connecting(peer)) {
        if ((retval = uds_attach(uds, peer)) != OK) return -retval;

        sockdriver_fire(&peer->sock, SEV_CONNECT);
    }

    uds_dequeue(uds, peer);

    uds_make_addr(peer->path, peer->path_len, addr, addrlen);
    conn = peer->conn;

    *newsockp = NULL;
    return sock_sockid(&conn->sock);
}

int uds_init(void)
{
    int i;

    printl("uds: UNIX domain socket driver is running.\n");

    sockdriver_init();

    idr_init(&sock_idr);

    for (i = 0; i < UDS_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&uds_hash_table[i]);
    }

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(uds_init);
    serv_init();

    sockdriver_task(&uds_driver, MAX_THREADS);

    return 0;
}
