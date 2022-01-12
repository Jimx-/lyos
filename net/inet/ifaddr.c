#include <assert.h>
#include <errno.h>

#include "inet.h"
#include "ifdev.h"

int ifaddr_v4_add(struct if_device* ifdev, const struct sockaddr_in* addr,
                  const struct sockaddr_in* mask,
                  const struct sockaddr_in* bcast,
                  const struct sockaddr_in* dest)
{
    ip_addr_t ipaddr, netmask, broad;
    struct netif* netif;
    u32 val;
    int retval;

    if ((retval = addr_get_inet((const struct sockaddr*)addr, sizeof(*addr),
                                IPADDR_TYPE_V4, &ipaddr, NULL)) != OK)
        return retval;

    val = ntohl(ip_addr_get_ip4_u32(&ipaddr));

    if (mask) {
        assert(FALSE);
    } else {
        if (IN_CLASSA(val))
            ip_addr_set_ip4_u32(&netmask, PP_HTONL(IN_CLASSA_NET));
        else if (IN_CLASSB(val))
            ip_addr_set_ip4_u32(&netmask, PP_HTONL(IN_CLASSB_NET));
        else if (IN_CLASSC(val))
            ip_addr_set_ip4_u32(&netmask, PP_HTONL(IN_CLASSC_NET));
        else
            ip_addr_set_ip4_u32(&netmask, PP_HTONL(0xf0000000));
    }

    netif = &ifdev->netif;

    if (!ifdev->v4_set || !ip_addr_cmp(netif_ip_addr4(netif), &ipaddr)) {
        ip4_addr_t ip4zero;

        if (ifdev->v4_set) return ENOBUFS;

        ip4_addr_set_zero(&ip4zero);

        netif_set_addr(netif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), &ip4zero);

        ifdev->v4_set = TRUE;
    } else {
        assert(FALSE);
    }

    return 0;
}

int ifaddr_v4_get(struct if_device* ifdev, unsigned int num,
                  struct sockaddr_in* addr, struct sockaddr_in* mask,
                  struct sockaddr_in* bcast, struct sockaddr_in* dest)
{
    struct netif* netif;
    socklen_t addr_len;

    if (num != 0 || !ifdev->v4_set) return EADDRNOTAVAIL;

    netif = &ifdev->netif;

    if (addr) {
        addr_len = sizeof(*addr);

        addr_set_inet((struct sockaddr*)addr, &addr_len, netif_ip_addr4(netif),
                      0);
    }

    if (mask) {
        addr_len = sizeof(*mask);

        addr_set_inet((struct sockaddr*)mask, &addr_len,
                      netif_ip_netmask4(netif), 0);
    }

    if (bcast) {
        bcast->sin_family = AF_UNSPEC;
    }

    if (dest) {
        bcast->sin_family = AF_UNSPEC;
    }

    return 0;
}

void ifaddr_hwaddr_get(struct if_device* ifdev, unsigned int num,
                       struct sockaddr* addr)
{
    const uint8_t* hwaddr;
    size_t hwaddr_len;
    socklen_t addr_len;

    if ((hwaddr_len = ifdev->hwaddr_len) > 0)
        hwaddr = ifdev->netif.hwaddr;
    else
        hwaddr = NULL;

    addr_len = sizeof(*addr);

    addr_set_link(addr, &addr_len, ifdev->type, hwaddr, hwaddr_len);
}
