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
#include <lyos/eventpoll.h>

#include "netlink.h"

#define MAX_THREADS 128

static struct idr sock_idr;

static struct nltable nl_table[MAX_LINKS];

static sockid_t netlink_socket(endpoint_t src, int domain, int type,
                               int protocol, struct sock** sock,
                               const struct sockdriver_ops** ops);
static int netlink_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags);
static __poll_t netlink_poll(struct sock* sock);
static int netlink_close(struct sock* sock, int force, int non_block);
static ssize_t netlink_recv(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t* ctl_len, struct sockaddr* addr,
                            socklen_t* addr_len, endpoint_t user_endpt,
                            int flags, int* rflags);
static int netlink_getsockname(struct sock* sock, struct sockaddr* addr,
                               socklen_t* addr_len);
static void netlink_free(struct sock* sock);

static void netlink_other(MESSAGE* msg);

static const struct sockdriver netlink_driver = {
    .sd_create = netlink_socket,
    .sd_other = netlink_other,
};

static const struct sockdriver_ops netlink_ops = {
    .sop_bind = netlink_bind,
    .sop_recv = netlink_recv,
    .sop_poll = netlink_poll,
    .sop_getsockname = netlink_getsockname,
    .sop_close = netlink_close,
    .sop_free = netlink_free,
};

static inline u32 netlink_group_mask(u32 group)
{
    return group ? 1 << (group - 1) : 0;
}

static inline unsigned int nlsock_gethash(int portid)
{
    return portid & NLS_HASH_MASK;
}

static inline void nlsock_addhash(struct nltable* table, struct nlsock* nls)
{
    unsigned int hash = nlsock_gethash(nls->portid);
    list_add(&nls->hash, &table->hash_table[hash]);
}

static inline void nlsock_unhash(struct nlsock* nls) { list_del(&nls->hash); }

static inline struct nlsock* nlsock_lookup(int protocol, u32 portid)
{
    struct nltable* table;
    struct nlsock* nls;
    unsigned int hash = nlsock_gethash(portid);

    assert(protocol >= 0 && protocol < MAX_LINKS);
    table = &nl_table[protocol];

    list_for_each_entry(nls, &table->hash_table[hash], hash)
    {
        if (nls->portid == portid) return nls;
    }

    return NULL;
}

static void netlink_update_listeners(struct nlsock* nls)
{
    struct nltable* table = &nl_table[nls_get_protocol(nls)];
    int i, bit;

    if (!table->listeners) return;

    for (i = 0; i < table->groups; i++) {
        bit = 0;
        list_for_each_entry(nls, &table->mc_list, node) bit |=
            GET_BIT(nls->groups, i);

        if (bit)
            SET_BIT(table->listeners, i);
        else
            UNSET_BIT(table->listeners, i);
    }
}

static int netlink_insert(struct nlsock* nls, u32 portid)
{
    int retval;

    retval = (portid == nls->portid) ? 0 : EBUSY;
    if (nls->bound) return retval;

    nls->portid = portid;

    if (nlsock_lookup(nls_get_protocol(nls), portid)) return EADDRINUSE;
    nlsock_addhash(&nl_table[nls_get_protocol(nls)], nls);

    nls->bound = portid;
    return 0;
}

static void netlink_remove(struct nlsock* nls)
{
    if (nlsock_lookup(nls_get_protocol(nls), nls->portid) == nls)
        nlsock_unhash(nls);

    if (nls->subscriptions) {
        list_del(&nls->node);
        netlink_update_listeners(nls);
    }
}

static void netlink_free(struct sock* sock)
{
    struct nlsock* nls = to_nlsock(sock);

    free(nls);
}

static sockid_t __netlink_socket(endpoint_t src, int domain, int type,
                                 int protocol, struct sock** sock,
                                 const struct sockdriver_ops** ops, int kernel)
{
    struct nlsock* nls;
    struct nltable* table;
    sockid_t id;

    if (domain != PF_NETLINK) return -EAFNOSUPPORT;

    switch (type) {
    case SOCK_DGRAM:
    case SOCK_RAW:
        break;
    default:
        return -EPROTOTYPE;
    }

    if (protocol < 0 || protocol >= MAX_LINKS) return -EPROTONOSUPPORT;

    table = &nl_table[protocol];
    if (!kernel && !table->registered) return -EPROTONOSUPPORT;

    nls = malloc(sizeof(*nls));
    if (!nls) return -ENOMEM;

    memset(nls, 0, sizeof(*nls));

    id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (id < 0) {
        netlink_free(&nls->sock);
    } else {
        *sock = &nls->sock;
        *ops = &netlink_ops;
    }

    return id;
}

static sockid_t netlink_socket(endpoint_t src, int domain, int type,
                               int protocol, struct sock** sock,
                               const struct sockdriver_ops** ops)
{
    return __netlink_socket(src, domain, type, protocol, sock, ops,
                            FALSE /* kernel */);
}

static sockid_t netlink_kernel_socket(endpoint_t src, int domain, int type,
                                      int protocol, struct sock** sock,
                                      const struct sockdriver_ops** ops)
{
    return __netlink_socket(src, domain, type, protocol, sock, ops,
                            TRUE /* kernel */);
}

static const struct sockdriver netlink_kernel_driver = {
    .sd_create = netlink_kernel_socket,
};

static sockid_t netlink_create(int unit, int groups)
{
    struct sock* sock;
    bitchunk_t* listeners;
    struct nltable* table;
    sockid_t sock_id;
    int retval;

    if ((sock_id = sockdriver_create_socket(&netlink_kernel_driver, KERNEL,
                                            PF_NETLINK, SOCK_DGRAM, unit)) < 0)
        return sock_id;

    if (!groups || groups < 32) groups = 32;

    sock = sock_get(sock_id);
    assert(sock);

    retval = -ENOMEM;
    listeners = malloc(BITCHUNKS(groups));
    if (!listeners) goto out_free_sock;

    if ((retval = netlink_insert(to_nlsock(sock), 0)) != OK) {
        retval = -retval;
        goto out_free_sock;
    }

    table = &nl_table[unit];
    if (!table->registered) {
        table->groups = groups;
        table->listeners = listeners;
        table->registered = 1;
    } else {
        free(listeners);
        table->registered++;
    }

    return sock_id;

out_free_sock:
    sock_free(sock);
    return retval;
}

static int netlink_realloc_groups(struct nlsock* nls)
{
    size_t groups;
    bitchunk_t* new_groups;

    groups = nl_table[nls_get_protocol(nls)].groups;
    if (!nl_table[nls_get_protocol(nls)].registered) return ENOENT;

    if (nls->ngroups >= groups) return 0;

    new_groups = realloc(nls->groups, BITCHUNKS(groups));
    if (!new_groups) return ENOMEM;

    memset(&new_groups[BITCHUNKS(nls->ngroups)], 0,
           BITCHUNKS(groups) - BITCHUNKS(nls->ngroups));

    nls->groups = new_groups;
    nls->ngroups = groups;

    return 0;
}

static void netlink_update_subscription(struct nlsock* nls,
                                        unsigned subscriptions)
{
    if (nls->subscriptions && !subscriptions)
        list_del(&nls->node);
    else if (!nls->subscriptions && subscriptions)
        list_add(&nls->node, &nl_table[nls_get_protocol(nls)].mc_list);
    nls->subscriptions = subscriptions;
}

static int netlink_autobind(struct nlsock* nls, endpoint_t user_endpt)
{
    static s32 rover = -4096;
    s32 portid;
    int ok;
    int retval;

    if (rover > -4096) rover = -4096;

    portid = get_epinfo(user_endpt, NULL, NULL);
    if (portid < 0) portid = --rover;

retry:
    ok = !nlsock_lookup(sock_protocol(&nls->sock), portid);
    if (!ok) {
        if (rover > -4096) rover = -4096;

        portid = --rover;
        goto retry;
    }

    retval = netlink_insert(nls, portid);
    if (retval == EADDRINUSE) goto retry;

    if (retval == EBUSY) retval = 0;
    return retval;
}

static int netlink_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags)
{
    struct nlsock* nls = to_nlsock(sock);
    struct sockaddr_nl* nladdr = (struct sockaddr_nl*)addr;
    u32 bound;
    int groups;
    int retval;

    if (addrlen < sizeof(struct sockaddr_nl)) return EINVAL;
    if (nladdr->nl_family != AF_NETLINK) return EINVAL;

    groups = nladdr->nl_groups;

    if (groups) {
        retval = netlink_realloc_groups(nls);
        if (retval) return retval;
    }

    if (nls->ngroups < BITCHUNK_BITS) groups &= (1UL << nls->ngroups) - 1;

    bound = nls->bound;
    if (bound) {
        if (nladdr->nl_pid != nls->portid) return EINVAL;
    }

    if (!bound) {
        retval = nladdr->nl_pid ? netlink_insert(nls, nladdr->nl_pid)
                                : netlink_autobind(nls, user_endpt);
        if (retval != OK) return retval;
    }

    if (!groups && (nls->groups == NULL || !(u32)nls->groups[0])) return 0;

    int i, new_group_count = 0, cur_group_count = 0;
    for (i = 0; i < 32; i++) {
        if (groups & (1UL << i)) new_group_count++;
        if (GET_BIT(nls->groups, i)) cur_group_count++;
    }

    netlink_update_subscription(nls, nls->subscriptions + new_group_count -
                                         cur_group_count);
    nls->groups[0] = (nls->groups[0] & ~0xffffffffUL) | groups;
    netlink_update_listeners(nls);

    return 0;
}

static ssize_t netlink_recv(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t* ctl_len, struct sockaddr* addr,
                            socklen_t* addr_len, endpoint_t user_endpt,
                            int flags, int* rflags)
{
    struct nlsock* nls = to_nlsock(sock);
    struct sk_buff* skb;
    struct sockaddr_nl* nladdr;
    size_t copied;
    int retval;

    if (flags & MSG_OOB) return -EOPNOTSUPP;

    copied = 0;
    retval = 0;
    *rflags = 0;

    for (;;) {
        skb = skb_peek(&nls->sock.recvq);

        if (!skb) {
            /* receiving data should take precedence over socket errors */
            if ((retval = sock_error(&nls->sock)) != OK) {
                retval = -retval;
                break;
            }

            /* need to wait for data now */
            if (flags & MSG_DONTWAIT) {
                retval = -EAGAIN;
                break;
            }

            sockdriver_suspend(&nls->sock, SEV_RECV);
            continue;
        }

        skb_unlink(skb, &nls->sock.recvq);
        break;
    }

    if (!skb) goto out;

    copied = skb->data_len;
    if (len < copied) {
        *rflags |= MSG_TRUNC;
        copied = len;
    }

    retval = skb_copy_to_iter(skb, 0, iter, copied);
    if (retval != OK) retval = -retval;

    if (addr) {
        nladdr = (struct sockaddr_nl*)addr;
        nladdr->nl_family = AF_NETLINK;
        nladdr->nl_pad = 0;
        nladdr->nl_pid = NETLINK_CB(skb).portid;
        nladdr->nl_groups = netlink_group_mask(NETLINK_CB(skb).dest_group);
        *addr_len = sizeof(struct sockaddr_nl);
    }

    skb_free(skb);

out:
    return retval ?: copied;
}

static __poll_t netlink_poll(struct sock* sock)
{
    struct nlsock* nls = to_nlsock(sock);
    __poll_t mask = 0;

    if (nls->sock.err) mask |= EPOLLERR;
    if (nls_is_shutdown(nls, SFL_SHUT_RD) && nls_is_shutdown(nls, SFL_SHUT_WR))
        mask |= EPOLLHUP;
    if (nls_is_shutdown(nls, SFL_SHUT_RD))
        mask |= EPOLLRDHUP | EPOLLIN | EPOLLRDNORM;

    if (!skb_queue_empty(&sock_recvq(&nls->sock)))
        mask |= EPOLLIN | EPOLLRDNORM;

    mask |= EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND;

    return mask;
}

static int netlink_getsockname(struct sock* sock, struct sockaddr* addr,
                               socklen_t* addr_len)
{
    struct nlsock* nls = to_nlsock(sock);
    struct sockaddr_nl* nladdr = (struct sockaddr_nl*)addr;

    nladdr->nl_family = AF_NETLINK;
    nladdr->nl_pad = 0;
    nladdr->nl_pid = nls->portid;
    nladdr->nl_groups = nls->groups ? nls->groups[0] : 0;

    *addr_len = sizeof(*nladdr);
    return 0;
}

static int netlink_close(struct sock* sock, int force, int non_block)
{
    struct nlsock* nls = to_nlsock(sock);

    netlink_remove(nls);
    return 0;
}

static int netlink_send_skb(struct sock* sock, struct sk_buff* skb)
{
    int len = skb->data_len;

    skb_queue_tail(&sock_recvq(sock), skb);
    sockdriver_fire(sock, SEV_RECV);

    return len;
}

static void netlink_broadcast_one(struct nlsock* nls, struct sk_buff* skb,
                                  u32 portid, u32 group, int* delivered)
{
    if (nls->portid == portid || group - 1 >= nls->ngroups ||
        !GET_BIT(nls->groups, group - 1))
        return;

    skb = skb_get(skb);
    skb_orphan(skb);

    netlink_send_skb(&nls->sock, skb);
    *delivered = 1;
}

static int netlink_broadcast(struct sock* sock, struct sk_buff* skb, u32 portid,
                             u32 group)
{
    struct nlsock* nls = to_nlsock(sock);
    struct nlsock* nlsk;
    int delivered = 0;

    NETLINK_CB(skb).portid = portid;
    NETLINK_CB(skb).dest_group = group;

    list_for_each_entry(nlsk, &nl_table[nls_get_protocol(nls)].mc_list, node)
    {
        if (nlsk != nls)
            netlink_broadcast_one(nlsk, skb, portid, group, &delivered);
    }

    if (delivered) return 0;

    return -ESRCH;
}

void do_netlink_create(MESSAGE* msg)
{
    struct netlink_create_req* req =
        (struct netlink_create_req*)msg->MSG_PAYLOAD;
    struct netlink_resp* resp = (struct netlink_resp*)msg->MSG_PAYLOAD;
    int unit = req->unit;
    int groups = req->groups;

    resp->status = netlink_create(unit, groups);
}

void do_netlink_broadcast(MESSAGE* msg)
{
    struct netlink_transfer_req* req =
        (struct netlink_transfer_req*)msg->MSG_PAYLOAD;
    struct netlink_resp* resp = (struct netlink_resp*)msg->MSG_PAYLOAD;
    sockid_t sock_id = req->sock_id;
    u32 portid = req->portid;
    u32 group = req->group;
    mgrant_id_t grant = req->grant;
    size_t len = req->len;
    struct iovec_grant data_iov;
    struct iov_grant_iter data_iter;
    struct sock* sock;
    struct sk_buff* skb;
    int retval;

    data_iov.iov_grant = grant;
    data_iov.iov_len = len;
    iov_grant_iter_init(&data_iter, msg->source, &data_iov, 1, len);

    retval = -EINVAL;
    sock = sock_get(sock_id);
    if (!sock) goto out;

    if ((retval = skb_alloc(sock, len, &skb)) != OK) {
        retval = -retval;
        goto out;
    }

    if ((retval = skb_copy_from_iter(skb, 0, &data_iter, len)) != OK) {
        retval = -retval;
        goto out_free_skb;
    }

    retval = netlink_broadcast(sock, skb, portid, group);

out_free_skb:
    skb_free(skb);
out:
    resp->status = retval;
}

static void netlink_other(MESSAGE* msg)
{
    int reply = TRUE;

    switch (msg->type) {
    case NETLINK_CREATE:
        do_netlink_create(msg);
        break;
    case NETLINK_BROADCAST:
        do_netlink_broadcast(msg);
        break;
    }

    if (reply) {
        msg->type = SYSCALL_RET;
        send_recv(SEND, msg->source, msg);
    }
}

static int netlink_init(void)
{
    int i, j;

    printl("netlink: Netlink socket driver is running.\n");

    sockdriver_init();

    idr_init(&sock_idr);

    for (i = 0; i < MAX_LINKS; i++) {
        nl_table[i].registered = 0;

        INIT_LIST_HEAD(&nl_table[i].mc_list);
        for (j = 0; j < NLS_HASH_SIZE; j++)
            INIT_LIST_HEAD(&nl_table[i].hash_table[j]);
    }

    return 0;
}

int main()
{
    netlink_init();

    sockdriver_task(&netlink_driver, MAX_THREADS);

    return 0;
}
