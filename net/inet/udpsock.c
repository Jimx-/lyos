#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lyos/idr.h>
#include <assert.h>
#include <lyos/sysutils.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

#include <lwip/udp.h>

#include "inet.h"
#include "ifdev.h"
#include "ifaddr.h"
#include "ifdev.h"
#include "pktsock.h"

#define UDP_DEF_SNDBUF 8192
#define UDP_DEF_RCVBUF 32768

#define UDP_MAX_PAYLOAD (UINT16_MAX)

struct udpsock {
    struct pktsock pktsock;
    struct udp_pcb* pcb;
};

#define to_udpsock(sock) ((struct udpsock*)(sock))

#define udpsock_get_ipsock(sock) (pktsock_get_ipsock(&((sock)->pktsock)))
#define udpsock_is_conn(udp)     (udp_flags((udp)->pcb) & UDP_FLAGS_CONNECTED)

extern struct idr sock_idr;

static int udpsock_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags);
static int udpsock_connect(struct sock* sock, struct sockaddr* addr,
                           size_t addrlen, endpoint_t user_endpt, int flags);
static ssize_t udpsock_send(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t ctl_len, const struct sockaddr* addr,
                            socklen_t addr_len, endpoint_t user_endpt,
                            int flags);
static int udpsock_ioctl(struct sock* sock, unsigned long request,
                         const struct sockdriver_data* data,
                         endpoint_t user_endpt, int flags);
static int udpsock_close(struct sock* sock, int force);
static void udpsock_free(struct sock* sock);

const struct sockdriver_ops udpsock_ops = {
    .sop_bind = udpsock_bind,
    .sop_connect = udpsock_connect,
    .sop_send = udpsock_send,
    .sop_recv = pktsock_recv,
    .sop_ioctl = udpsock_ioctl,
    .sop_poll = pktsock_poll,
    .sop_close = udpsock_close,
    .sop_free = udpsock_free,
};

static void udpsock_input(void* arg, struct udp_pcb* pcb, struct pbuf* pbuf,
                          const ip_addr_t* ipaddr, uint16_t port)
{
    struct udpsock* udp = (struct udpsock*)arg;

    pktsock_input(&udp->pktsock, pbuf, ipaddr, port);
}

sockid_t udpsock_socket(int domain, int protocol, struct sock** sock,
                        const struct sockdriver_ops** ops)
{
    struct udpsock* udp;
    sockid_t sock_id;
    int type;

    switch (protocol) {
    case 0:
    case IPPROTO_UDP:
        break;
    default:
        return -EPROTONOSUPPORT;
    }

    sock_id = idr_alloc(&sock_idr, NULL, 1, 0);
    if (sock_id < 0) return sock_id;

    udp = malloc(sizeof(*udp));
    if (!udp) return -ENOMEM;

    memset(udp, 0, sizeof(*udp));

    type = pktsock_socket(&udp->pktsock, domain, UDP_DEF_SNDBUF, UDP_DEF_RCVBUF,
                          sock);

    if ((udp->pcb = udp_new_ip_type(type)) == NULL) return -ENOMEM;
    udp_recv(udp->pcb, udpsock_input, udp);

    *ops = &udpsock_ops;
    return sock_id;
}

static int udpsock_bind(struct sock* sock, struct sockaddr* addr,
                        size_t addrlen, endpoint_t user_endpt, int flags)
{
    struct udpsock* udp = to_udpsock(sock);
    ip_addr_t ipaddr;
    uint16_t port;
    err_t err;
    int retval;

    if ((retval = ipsock_get_src_addr(udpsock_get_ipsock(udp), addr, addrlen,
                                      user_endpt, &udp->pcb->local_ip,
                                      udp->pcb->local_port, TRUE, &ipaddr,
                                      &port)) != 0)
        return retval;

    err = udp_bind(udp->pcb, &ipaddr, port);
    return convert_err(err);
}

static int udpsock_connect(struct sock* sock, struct sockaddr* addr,
                           size_t addrlen, endpoint_t user_endpt, int flags)
{
    struct udpsock* udp = to_udpsock(sock);
    ip_addr_t dst_addr;
    u16 dst_port;
    err_t err;
    int retval;

    if (addr_is_unspec(addr, addrlen)) {
        udp_disconnect(udp->pcb);
        return 0;
    }

    if ((retval = ipsock_get_dst_addr(udpsock_get_ipsock(udp), addr, addrlen,
                                      &udp->pcb->local_ip, &dst_addr,
                                      &dst_port)) != 0)
        return retval;

    err = udp_connect(udp->pcb, &dst_addr, dst_port);
    return convert_err(err);
}

static ssize_t udpsock_send(struct sock* sock, struct iov_grant_iter* iter,
                            size_t len, const struct sockdriver_data* ctl,
                            socklen_t ctl_len, const struct sockaddr* addr,
                            socklen_t addr_len, endpoint_t user_endpt,
                            int flags)
{
    struct udpsock* udp = to_udpsock(sock);
    struct pbuf* pbuf;
    struct if_device* ifdev = NULL;
    struct netif* netif;
    const ip_addr_t *src_addrp, *dst_addrp;
    ip_addr_t dst_addr;
    uint16_t dst_port;
    size_t hdrlen;
    err_t err;
    int retval;

    src_addrp = &udp->pcb->local_ip;
    if (ip_addr_ismulticast(src_addrp))
        src_addrp = IP46_ADDR_ANY(IP_GET_TYPE(src_addrp));

    if (!udpsock_is_conn(udp)) {
        if ((retval =
                 ipsock_get_dst_addr(udpsock_get_ipsock(udp), addr, addr_len,
                                     src_addrp, &dst_addr, &dst_port)) != OK)
            return -retval;

        dst_addrp = &dst_addr;
    } else {
        dst_addrp = &udp->pcb->remote_ip;
        dst_port = udp->pcb->remote_port;
    }

    if (ifdev == NULL) {
        if (!(flags & MSG_DONTROUTE)) {
            if (IP_IS_ANY_TYPE_VAL(*src_addrp))
                src_addrp = IP46_ADDR_ANY(IP_GET_TYPE(dst_addrp));

            if ((netif = ip_route(src_addrp, dst_addrp)) == NULL)
                return -EHOSTUNREACH;

            ifdev = netif_get_ifdev(netif);
        }
    }

    if (ip_addr_isany(src_addrp)) {
        src_addrp = ifaddr_select(dst_addrp, ifdev, NULL);

        if (src_addrp == NULL) return -EHOSTUNREACH;
    }

    assert(len <= UDP_MAX_PAYLOAD);

    if (IP_IS_V6(dst_addrp))
        hdrlen = IP6_HLEN + UDP_HLEN;
    else
        hdrlen = IP_HLEN + UDP_HLEN;

    if (hdrlen + len > UDP_MAX_PAYLOAD) return -EMSGSIZE;

    if ((pbuf = pbuf_alloc(PBUF_RAW, len, PBUF_RAM)) == NULL) return -ENOBUFS;

    if ((retval = copy_socket_data(iter, len, pbuf, 0, TRUE)) < 0) {
        pbuf_free(pbuf);
        return retval;
    }

    if (ip_addr_ismulticast(dst_addrp))
        pbuf->flags |= PBUF_FLAG_LLMCAST;
    else if (ip_addr_isbroadcast(dst_addrp, &ifdev->netif))
        pbuf->flags |= PBUF_FLAG_LLBCAST;

    assert(!ip_addr_isany(src_addrp));
    assert(!ip_addr_ismulticast(src_addrp));

    err = udp_sendto_if_src(udp->pcb, pbuf, dst_addrp, dst_port, &ifdev->netif,
                            src_addrp);
    pbuf_free(pbuf);

    if ((retval = convert_err(err)) == 0) return len;
    return -retval;
}

static int udpsock_ioctl(struct sock* sock, unsigned long request,
                         const struct sockdriver_data* data,
                         endpoint_t user_endpt, int flags)
{
    return ifconf_ioctl(sock, request, data, user_endpt, flags);
}

static int udpsock_close(struct sock* sock, int force)
{
    struct udpsock* udp = to_udpsock(sock);

    udp_recv(udp->pcb, NULL, NULL);

    udp_remove(udp->pcb);
    udp->pcb = NULL;

    pktsock_close(&udp->pktsock);
    return 0;
}

static void udpsock_free(struct sock* sock)
{
    struct udpsock* udp = to_udpsock(sock);
    idr_remove(&sock_idr, sock_sockid(sock));
    free(udp);
}
