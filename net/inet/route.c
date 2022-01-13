#include <lyos/sysutils.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/route.h>

#include "inet.h"
#include "ifdev.h"
#include "ifaddr.h"

int route_add(const ip_addr_t* addr, unsigned int prefix,
              const ip_addr_t* gateway, struct if_device* ifdev,
              unsigned int flags)
{
    netif_set_gw(&ifdev->netif, gateway);

    if (ip_addr_isany(addr)) {
        netif_set_default(&ifdev->netif);
    }

    return 0;
}

static int route_process_inet(unsigned int type, const ip_addr_t* dst_addr,
                              const struct sockaddr* mask,
                              const struct sockaddr* gateway,
                              struct if_device* ifdev, unsigned int flags)
{
    ip_addr_t gw_storage, *gw_addr;
    socklen_t addr_len;
    unsigned int prefix;
    int retval;

    addr_len = IP_GET_TYPE(dst_addr) == IPADDR_TYPE_V4
                   ? sizeof(struct sockaddr_in)
                   : sizeof(struct sockaddr_in6);

    if (!(flags & RTF_HOST)) {
        if (mask == NULL) return EINVAL;

        if ((retval = addr_get_netmask(mask, addr_len, IP_GET_TYPE(dst_addr),
                                       &prefix, NULL)) != 0)
            return retval;
    } else {
        if (IP_IS_V4(dst_addr))
            prefix = 32;
        else
            prefix = 128;
    }

    if (type == RTMSG_NEWROUTE) {
        if (!(flags & RTF_UP)) return EINVAL;

        if ((flags & RTF_GATEWAY) && gateway) {
            if ((retval =
                     addr_get_inet(gateway, addr_len, IP_GET_TYPE(dst_addr),
                                   &gw_storage, NULL)) != 0)
                return retval;

            gw_addr = &gw_storage;
        }

        if (!ifdev) return ENETUNREACH;
    }

    switch (type) {
    case RTMSG_NEWROUTE:
        return route_add(dst_addr, prefix, gw_addr, ifdev, flags);
    }

    return EINVAL;
}

static int route_process(unsigned int type, const struct sockaddr* dst,
                         const struct sockaddr* mask,
                         const struct sockaddr* gateway, const char* ifname,
                         unsigned short flags)
{
    struct if_device* ifdev = NULL;
    ip_addr_t dst_addr;
    unsigned int addr_type;
    int retval;

    if (!dst) return EINVAL;

    switch (dst->sa_family) {
    case AF_INET:
        addr_type = IPADDR_TYPE_V4;
        break;
    default:
        return EAFNOSUPPORT;
    }

    if ((retval = addr_get_inet(dst, sizeof(*dst), addr_type, &dst_addr,
                                NULL)) != 0) {
        return retval;
    }

    if (type == RTMSG_NEWROUTE) {
        if (!ifname) return ENXIO;

        if ((ifdev = ifdev_find_by_name(ifname)) == NULL) return ENXIO;
    }

    return route_process_inet(type, &dst_addr, mask, gateway, ifdev, flags);
}

int route_ioctl(unsigned long request, const struct sockdriver_data* data,
                endpoint_t user_endpt, int flags)
{
    struct rtentry rtent;
    char name[IFNAMSIZ];
    unsigned int type;
    int retval;

    if ((retval = sockdriver_copyin(data, 0, &rtent, sizeof(rtent)) != 0))
        return retval;

    memset(name, 0, sizeof(name));
    if (rtent.rt_dev) {
        if ((retval = data_copy(SELF, name, user_endpt, rtent.rt_dev,
                                IFNAMSIZ)) != 0)
            return retval;
        name[IFNAMSIZ - 1] = '\0';
    }

    switch (request) {
    case SIOCADDRT:
        if (!is_superuser(user_endpt)) return EPERM;

        type = RTMSG_NEWROUTE;
        break;
    default:
        return ENOTTY;
    }

    retval =
        route_process(type, &rtent.rt_dst, &rtent.rt_genmask, &rtent.rt_gateway,
                      rtent.rt_dev ? name : NULL, rtent.rt_flags);

    return retval;
}
