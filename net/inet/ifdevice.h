#ifndef _INET_IFDEVICE_H_
#define _INET_IFDEVICE_H_

#include <lyos/list.h>
#include <lwip/netif.h>
#include <net/if.h>

struct if_device {
    struct list_head list;
    char name[IFNAMSIZ];
    struct netif netif;

    unsigned int mtu;
};

#endif
