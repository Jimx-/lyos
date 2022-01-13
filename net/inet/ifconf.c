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

#define LOOPBACK_IFNAME "lo"

void ifconf_init(void)
{
    int retval;
    struct if_device* loopback;
    const struct sockaddr_in addr = {.sin_family = AF_INET,
                                     .sin_addr = {htonl(INADDR_LOOPBACK)}};

    if ((retval = ifdev_create(LOOPBACK_IFNAME)) != 0)
        panic("inet: failed to create loopback interface (%d)", retval);

    loopback = ifdev_find_by_name(LOOPBACK_IFNAME);
    if (!loopback) panic("inet: cannot find loopback interface");

    if ((retval = ifaddr_v4_add(loopback, &addr, NULL, NULL, NULL)) != 0)
        panic("inet: failed to set IPv4 address on loopback interface (%d)",
              retval);

    if ((retval = ifdev_set_ifflags(loopback, IFF_UP)) != 0)
        panic("inet: failed to bring up loopback interface (%d)", retval);

    netif_set_default(&loopback->netif);
}

static int ifconf_ioctl_ifconf(unsigned long request,
                               const struct sockdriver_data* data,
                               endpoint_t user_endpt)
{
    struct ifconf ifconf;
    int retval;

    assert(request == SIOCGIFCONF);

    if ((retval = sockdriver_copyin(data, 0, &ifconf, sizeof(ifconf)) != 0))
        return retval;
    retval = ifdev_ifconf(&ifconf, sizeof(struct ifreq), user_endpt);
    if (retval) return retval;

    return sockdriver_copyout(data, 0, &ifconf, sizeof(ifconf));
}

static int ifconf_ioctl_ifreq(unsigned long request,
                              const struct sockdriver_data* data)
{
    struct ifreq ifr;
    struct if_device* ifdev;
    int retval;

    if ((retval = sockdriver_copyin(data, 0, &ifr, sizeof(ifr)) != 0))
        return retval;

    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ifdev = ifdev_find_by_name(ifr.ifr_name);
    if (!ifdev) return ENXIO;

    _Static_assert(sizeof(ifr.ifr_flags) == sizeof(short),
                   "ifr.ifr_flags is not short");

    switch (request) {
    case SIOCGIFFLAGS:
        ifr.ifr_flags = ifdev->ifflags;
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCSIFFLAGS:
        return ifdev_set_ifflags(ifdev, (unsigned int)ifr.ifr_flags & 0xffff);

    case SIOCGIFMETRIC:
        ifr.ifr_metric = ifdev->metric;
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCSIFMETRIC:
        ifdev->metric = ifr.ifr_metric;
        return 0;

    case SIOCGIFMTU:
        ifr.ifr_mtu = ifdev->mtu;
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCGIFINDEX:
        ifr.ifr_ifindex = ifdev_get_index(ifdev);
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCGIFHWADDR:
        ifaddr_hwaddr_get(ifdev, 0, &ifr.ifr_addr);
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCGIFTXQLEN:
        /* XXX */
        ifr.ifr_qlen = 0;
        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    default:
        return ENOTTY;
    }
}

static int ifconf_ioctl_v4_ifreq(unsigned long request,
                                 const struct sockdriver_data* data)
{
    struct sockaddr_in addr, mask, bcast, dest, *sin = NULL;
    struct ifreq ifr;
    struct if_device* ifdev;
    int retval;

    if ((retval = sockdriver_copyin(data, 0, &ifr, sizeof(ifr)) != 0))
        return retval;

    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ifdev = ifdev_find_by_name(ifr.ifr_name);
    if (!ifdev) return ENXIO;

    switch (request) {
    case SIOCGIFADDR:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
        switch (request) {
        case SIOCGIFADDR:
            sin = &addr;
            break;
        case SIOCGIFDSTADDR:
            sin = &dest;
            break;
        case SIOCGIFBRDADDR:
            sin = &bcast;
            break;
        case SIOCGIFNETMASK:
            sin = &mask;
            break;
        }

        if ((retval = ifaddr_v4_get(ifdev, 0, &addr, &mask, &bcast, &dest)) !=
            0)
            return retval;

        if (sin->sin_family == AF_UNSPEC) return EADDRNOTAVAIL;

        memcpy(&ifr.ifr_addr, sin, sizeof(*sin));

        return sockdriver_copyout(data, 0, &ifr, sizeof(ifr));

    case SIOCSIFADDR:
        sin = (struct sockaddr_in*)&ifr.ifr_addr;
        return ifaddr_v4_add(ifdev, sin, NULL, NULL, NULL);

    case SIOCSIFNETMASK:
    case SIOCSIFBRDADDR:
    case SIOCSIFDSTADDR:
        if ((retval = ifaddr_v4_get(ifdev, 0, &addr, &mask, &bcast, &dest)) !=
            0)
            return retval;

        sin = (struct sockaddr_in*)&ifr.ifr_addr;

        switch (request) {
        case SIOCSIFNETMASK:
            memcpy(&mask, sin, sizeof(mask));
            break;
        case SIOCSIFBRDADDR:
            memcpy(&bcast, sin, sizeof(bcast));
            break;
        case SIOCSIFDSTADDR:
            memcpy(&dest, sin, sizeof(dest));
            break;
        }

        return ifaddr_v4_add(ifdev, &addr, &mask, &bcast, &dest);

    default:
        return ENOTTY;
    }
}

static int ifconf_ioctl_v4(unsigned long request,
                           const struct sockdriver_data* data,
                           endpoint_t user_endpt)
{
    switch (request) {
    case SIOCSIFADDR:
    case SIOCSIFDSTADDR:
    case SIOCSIFBRDADDR:
    case SIOCSIFNETMASK:
        if (!is_superuser(user_endpt)) return EPERM;

    case SIOCGIFADDR:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
        return ifconf_ioctl_v4_ifreq(request, data);

    default:
        return ENOTTY;
    }
}

int ifconf_ioctl(struct sock* sock, unsigned long request,
                 const struct sockdriver_data* data, endpoint_t user_endpt,
                 int flags)
{
    int domain;

    domain = sock_domain(sock);

    switch (request) {
    case SIOCGIFCONF:
        return ifconf_ioctl_ifconf(request, data, user_endpt);

    case SIOCSIFFLAGS:
    case SIOCSIFMETRIC:
    case SIOCSIFMTU:
        if (!is_superuser(user_endpt)) return EPERM;

    case SIOCGIFFLAGS:
    case SIOCGIFMETRIC:
    case SIOCGIFMTU:
    case SIOCGIFINDEX:
    case SIOCGIFHWADDR:
    case SIOCGIFTXQLEN:
        return ifconf_ioctl_ifreq(request, data);

    case SIOCADDRT:
        return route_ioctl(request, data, user_endpt, flags);

    default:
        switch (domain) {
        case AF_INET:
            return ifconf_ioctl_v4(request, data, user_endpt);
        default:
            break;
        }
    }

    return ENOTTY;
}
