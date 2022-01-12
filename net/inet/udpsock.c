#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lyos/idr.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>

#include <lwip/udp.h>

#include "inet.h"
#include "ifdev.h"
#include "ipsock.h"

#define UDP_DEF_SNDBUF 8192
#define UDP_DEF_RCVBUF 32768

struct udpsock {
    struct ipsock ipsock;
    struct udp_pcb* pcb;
};

#define to_udpsock(sock) ((struct udpsock*)(sock))

extern struct idr sock_idr;

static int udpsock_ioctl(struct sock* sock, unsigned long request,
                         const struct sockdriver_data* data,
                         endpoint_t user_endpt, int flags);
static void udpsock_free(struct sock* sock);

const struct sockdriver_ops udpsock_ops = {
    .sop_ioctl = udpsock_ioctl,
    .sop_free = udpsock_free,
};

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

    type = ipsock_socket(&udp->ipsock, domain, UDP_DEF_SNDBUF, UDP_DEF_RCVBUF,
                         sock);

    if ((udp->pcb = udp_new_ip_type(type)) == NULL) return -ENOMEM;

    *ops = &udpsock_ops;
    return sock_id;
}

static int udpsock_ioctl(struct sock* sock, unsigned long request,
                         const struct sockdriver_data* data,
                         endpoint_t user_endpt, int flags)
{
    return ifconf_ioctl(sock, request, data, user_endpt, flags);
}

static void udpsock_free(struct sock* sock)
{
    struct udpsock* udp = to_udpsock(sock);
    idr_remove(&sock_idr, sock_sockid(sock));
    free(udp);
}
