#include <errno.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <sys/epoll.h>

#include "inet.h"
#include "ifdev.h"
#include "pktsock.h"

struct pkthdr {
    uint16_t port;
    uint8_t dstif;
    uint8_t addrif;
    uint8_t tos;
    uint8_t ttl;
    uint8_t flags;
    uint8_t _unused;
};

#define PKTHF_IPV6  0x01
#define PKTHF_MCAST 0x02
#define PKTHF_BCAST 0x04

int pktsock_socket(struct pktsock* pkt, int domain, size_t sndbuf,
                   size_t rcvbuf, struct sock** sock)
{
    pkt->rcvhead = NULL;
    pkt->rcvtailp = &pkt->rcvhead;
    pkt->rcvlen = 0;

    return ipsock_socket(&pkt->ipsock, domain, sndbuf, rcvbuf, sock);
}

static inline int pktsock_may_recv(struct pktsock* pkt, struct pbuf* pbuf)
{
    return (pkt->rcvlen + pbuf->tot_len <= ipsock_get_rcvbuf(&pkt->ipsock));
}

void pktsock_input(struct pktsock* pkt, struct pbuf* pbuf,
                   const ip_addr_t* srcaddr, uint16_t port)
{
    struct pktaddr4 pktaddr4;
    struct pkthdr pkthdr;
    void* pktaddr;
    size_t pktaddrlen;
    struct if_device* ifdev;

    if (pbuf->ref != 1) panic("input packet has multiple references!");

    if (!pktsock_may_recv(pkt, pbuf)) {
        pbuf_free(pbuf);
        return;
    }

    assert(IP_IS_V4(srcaddr));
    assert(ip4_current_dest_addr() != NULL);

    memcpy(&pktaddr4.srcaddr, ip_2_ip4(srcaddr), sizeof(pktaddr4.srcaddr));
    memcpy(&pktaddr4.dstaddr, ip4_current_dest_addr(),
           sizeof(pktaddr4.srcaddr));
    pktaddr = &pktaddr4;
    pktaddrlen = sizeof(pktaddr4);

    assert(pktaddrlen + sizeof(pkthdr) <= IP_HLEN);

    assert(ip_current_input_netif() != NULL);
    ifdev = netif_get_ifdev(ip_current_input_netif());
    pkthdr.dstif = (uint16_t)ifdev_get_index(ifdev);

    assert(ip_current_netif() != NULL);
    ifdev = netif_get_ifdev(ip_current_netif());
    pkthdr.addrif = (uint16_t)ifdev_get_index(ifdev);

    if ((pbuf->flags & PBUF_FLAG_LLMCAST) ||
        ip_addr_ismulticast(ip_current_dest_addr()))
        pkthdr.flags |= PKTHF_MCAST;
    else if ((pbuf->flags & PBUF_FLAG_LLBCAST) ||
             ip_addr_isbroadcast(ip_current_dest_addr(), ip_current_netif()))
        pkthdr.flags |= PKTHF_BCAST;

    pkthdr.port = port;

    pbuf_header(pbuf, sizeof(pkthdr));
    memcpy(pbuf->payload, &pkthdr, sizeof(pkthdr));

    pbuf_header(pbuf, pktaddrlen);
    memcpy(pbuf->payload, pktaddr, pktaddrlen);

    pbuf_header(pbuf, -(int)(sizeof(pkthdr) + pktaddrlen));

    *pkt->rcvtailp = pbuf;
    pkt->rcvtailp = &pbuf->next;
    pkt->rcvlen += pbuf->len;

    sockdriver_fire(ipsock_get_sock(&pkt->ipsock), SEV_RECV);
}

static void pktsock_dequeue(struct pktsock* pkt)
{
    struct pbuf *pbuf, **pnext;
    size_t size;

    pbuf = pkt->rcvhead;
    assert(pbuf != NULL);

    pnext = &pbuf->next;
    size = pbuf->len;

    if ((pkt->rcvhead = *pnext) == NULL) pkt->rcvtailp = &pkt->rcvhead;

    assert(pkt->rcvlen >= size);
    pkt->rcvlen -= size;

    *pnext = NULL;
    pbuf_free(pbuf);
}

ssize_t pktsock_recv(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                     const struct sockdriver_data* ctl, socklen_t* ctl_len,
                     struct sockaddr* addr, socklen_t* addr_len,
                     endpoint_t user_endpt, int flags, int* rflags)
{
    struct pktsock* pkt = (struct pktsock*)sock;
    struct pktaddr4 pktaddr4;
    void* pktaddr;
    struct pkthdr pkthdr;
    struct pbuf* pbuf;
    ip_addr_t srcaddr;
    int retval;

    while ((pbuf = pkt->rcvhead) == NULL)
        sockdriver_suspend(sock, SEV_RECV);

    pbuf_header(pbuf, sizeof(pkthdr));
    memcpy(&pkthdr, pbuf->payload, sizeof(pkthdr));

    assert(!(pkthdr.flags & PKTHF_IPV6));
    pbuf_header(pbuf, sizeof(pktaddr4));
    memcpy(&pktaddr4, pbuf->payload, sizeof(pktaddr4));
    pktaddr = &pktaddr4;

    ip_addr_copy_from_ip4(srcaddr, pktaddr4.srcaddr);

    pbuf_header(pbuf, -(int)(sizeof(pkthdr) + sizeof(pktaddr4)));

    if (len >= pbuf->tot_len)
        len = pbuf->tot_len;
    else
        *rflags |= MSG_TRUNC;

    retval = copy_socket_data(iter, len, pbuf, 0, FALSE);
    if (retval < 0) return retval;

    ipsock_set_addr(&pkt->ipsock, addr, addr_len, &srcaddr, pkthdr.port);

    if (!(flags & MSG_PEEK)) pktsock_dequeue(pkt);

    if (ctl_len) *ctl_len = 0;

    return len;
}

__poll_t pktsock_poll(struct sock* sock)
{
    struct pktsock* pkt = (struct pktsock*)sock;
    __poll_t mask = 0;

    mask |= EPOLLOUT | EPOLLWRNORM;
    if (pkt->rcvhead != NULL) {
        mask |= EPOLLIN | EPOLLRDNORM;
    }

    return mask;
}

static void pktsock_drain(struct pktsock* pkt)
{

    while (pkt->rcvhead != NULL)
        pktsock_dequeue(pkt);

    assert(pkt->rcvlen == 0);
    assert(pkt->rcvtailp == &pkt->rcvhead);
}

void pktsock_close(struct pktsock* pkt) { pktsock_drain(pkt); }
