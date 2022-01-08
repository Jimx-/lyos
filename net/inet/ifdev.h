#ifndef _INET_IFDEV_H_
#define _INET_IFDEV_H_

#include <lyos/list.h>
#include <lwip/netif.h>
#include <net/if.h>

struct if_device;

struct if_device_stats {
    unsigned long rx_packets;
    unsigned long tx_packets;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
};

struct if_device_operations {
    err_t (*ido_init)(struct if_device* ifdev, struct netif* netif);
    err_t (*ido_input)(struct pbuf* pbuf, struct netif* netif);
    void (*ido_poll)(struct if_device* ifdev);
};

struct if_device {
    struct list_head list;
    struct netif netif;

    char name[IFNAMSIZ];
    unsigned int ifflags;

    unsigned int hwaddr_len;
    unsigned int mtu;

    int v4_set;

    struct if_device_stats stats;

    const struct if_device_operations* ifd_ops;
};

void ifdev_poll(void);

void ifdev_register(const char* name, int (*create)(const char*));
int ifdev_create(const char* name);

void ifdev_add(struct if_device* ifdev, const char* name, unsigned int ifflags,
               unsigned int hwaddr_len, unsigned int mtu,
               const struct if_device_operations* ifd_ops);

struct if_device* ifdev_find_by_name(const char* name);

void ifdev_input(struct if_device* ifdev, struct pbuf* pbuf,
                 struct netif* netif);

void ifdev_update_hwaddr(struct if_device* ifdev, const u8* hwaddr);

void ifdev_update_ifflags(struct if_device* ifdev, unsigned int ifflags);
int ifdev_set_ifflags(struct if_device* ifdev, unsigned int ifflags);

#endif
