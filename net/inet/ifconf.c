#include <lyos/sysutils.h>

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
