#include <errno.h>

#include "inet.h"
#include "ipsock.h"

static int ipsock_get_type(struct ipsock* ip)
{
    if (!(ip->flags & IPF_IPV6))
        return IPADDR_TYPE_V4;
    else
        return IPADDR_TYPE_ANY;
}

int ipsock_socket(struct ipsock* ip, int domain, size_t sndbuf, size_t rcvbuf,
                  struct sock** sockp)
{
    ip->flags = (domain == AF_INET6) ? IPF_IPV6 : 0;

    ip->sndbuf = sndbuf;
    ip->rcvbuf = rcvbuf;

    *sockp = &ip->sock;

    return ipsock_get_type(ip);
}

void ipsock_clone(struct ipsock* ip, struct ipsock* newip, sockid_t newid)
{
    sockdriver_clone(&ip->sock, &newip->sock, newid);

    newip->flags = ip->flags;
    newip->sndbuf = ip->sndbuf;
    newip->rcvbuf = ip->rcvbuf;
}

int ipsock_get_src_addr(struct ipsock* ip, const struct sockaddr* addr,
                        socklen_t addr_len, endpoint_t user_endpt,
                        ip_addr_t* local_ip, uint16_t local_port,
                        int allow_mcast, ip_addr_t* src_addr,
                        uint16_t* src_port)
{
    uint16_t port;
    int retval;

    if (local_port) return EINVAL;

    if ((retval = addr_get_inet(addr, addr_len, ipsock_get_type(ip), src_addr,
                                &port)) != 0)
        return retval;

    if (src_port) {
        if (port && port < IPPORT_RESERVED && !is_superuser(user_endpt))
            return EACCES;

        *src_port = port;
    }

    return 0;
}

int ipsock_get_dst_addr(struct ipsock* ip, const struct sockaddr* addr,
                        socklen_t addr_len, ip_addr_t* local_ip,
                        ip_addr_t* dst_addr, uint16_t* dst_port)
{
    uint16_t port;
    int retval;

    if ((retval = addr_get_inet(addr, addr_len, ipsock_get_type(ip), dst_addr,
                                &port)) != 0)
        return retval;

    if (IP_GET_TYPE(dst_addr) == IPADDR_TYPE_ANY)
        IP_SET_TYPE(dst_addr, IPADDR_TYPE_V6);

    if (IP_GET_TYPE(local_ip) != IPADDR_TYPE_ANY &&
        IP_IS_V6(local_ip) != IP_IS_V6(dst_addr))
        return EINVAL;

    if (dst_port) {
        if (!port) return EADDRNOTAVAIL;

        *dst_port = port;
    }

    return 0;
}

void ipsock_set_addr(struct ipsock* ip, struct sockaddr* addr,
                     socklen_t* addr_len, ip_addr_t* ipaddr, uint16_t port)
{
    *addr_len = SOCKADDR_MAX;
    addr_set_inet(addr, addr_len, ipaddr, port);
}
