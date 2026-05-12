#ifndef _INET_PKTSOCK_H_
#define _INET_PKTSOCK_H_

#include "ipsock.h"

struct pktsock {
    struct ipsock ipsock;
    struct pbuf* rcvhead;
    struct pbuf** rcvtailp;
    size_t rcvlen;
};

struct pktaddr4 {
    ip4_addr_t srcaddr;
    ip4_addr_t dstaddr;
};

#define pktsock_get_ipsock(pkt) &((pkt)->ipsock)

int pktsock_socket(struct pktsock* pkt, int domain, size_t sndbuf,
                   size_t rcvbuf, struct sock** sock);
void pktsock_input(struct pktsock* pkt, struct pbuf* pbuf,
                   const ip_addr_t* srcaddr, uint16_t port);
ssize_t pktsock_recv(struct sock* sock, struct iov_grant_iter* iter, size_t len,
                     const struct sockdriver_data* ctl, socklen_t* ctl_len,
                     struct sockaddr* addr, socklen_t* addr_len,
                     endpoint_t user_endpt, int flags, int* rflags);
__poll_t pktsock_poll(struct sock* sock);
void pktsock_close(struct pktsock* pkt);

#endif
