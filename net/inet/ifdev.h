#ifndef _INET_IFDEV_H_
#define _INET_IFDEV_H_

#include <lyos/list.h>
#include <lwip/netif.h>
#include <net/if.h>

#define LINK_STATE_UNKNOWN 0
#define LINK_STATE_UP      1
#define LINK_STATE_DOWN    2

struct if_device;

struct if_device_stats {
    unsigned long rx_packets;
    unsigned long tx_packets;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long rx_dropped;
    unsigned long tx_dropped;
};

struct if_device_operations {
    err_t (*ido_init)(struct if_device* ifdev, struct netif* netif);
    err_t (*ido_input)(struct pbuf* pbuf, struct netif* netif);
    err_t (*ido_output)(struct if_device* ifdev, struct pbuf* pbuf,
                        struct netif* netif);
    err_t (*ido_output_v4)(struct netif* netif, struct pbuf* pbuf,
                           const ip4_addr_t* ipaddr);
    err_t (*ido_output_v6)(struct netif* netif, struct pbuf* pbuf,
                           const ip6_addr_t* ipaddr);
    void (*ido_poll)(struct if_device* ifdev);
};

struct if_device {
    struct list_head list;
    struct netif netif;

    char name[IFNAMSIZ];
    unsigned int ifflags;
    unsigned int link_state;

    unsigned int hwaddr_len;
    unsigned int mtu;

    int v4_set;

    struct if_device_stats stats;

    const struct if_device_operations* ifd_ops;
};

#define ifdev_is_loopback(ifdev) ((ifdev)->ifflags & IFF_LOOPBACK)
#define ifdev_is_up(ifdev)       ((ifdev)->ifflags & IFF_UP)
#define ifdev_is_link_up(ifdev)  (netif_is_link_up(&(ifdev)->netif))
#define ifdev_get_index(ifdev)   ((uint32_t)(netif_get_index(&(ifdev)->netif)))

#define netif_get_ifdev(netif) ((struct if_device*)(netif)->state)

static inline void ifdev_output_drop(struct if_device* ifdev)
{
    ifdev->stats.tx_dropped++;
}

void ifdev_poll(void);

void ifdev_register(const char* name, int (*create)(const char*));
int ifdev_create(const char* name);

void ifdev_add(struct if_device* ifdev, const char* name, unsigned int ifflags,
               unsigned int hwaddr_len, unsigned int mtu,
               const struct if_device_operations* ifd_ops);

struct if_device* ifdev_find_by_index(unsigned int index);
struct if_device* ifdev_find_by_name(const char* name);

void ifdev_input(struct if_device* ifdev, struct pbuf* pbuf,
                 struct netif* netif);

void ifdev_update_hwaddr(struct if_device* ifdev, const u8* hwaddr);

void ifdev_update_ifflags(struct if_device* ifdev, unsigned int ifflags);
int ifdev_set_ifflags(struct if_device* ifdev, unsigned int ifflags);

void ifdev_update_link(struct if_device* ifdev, unsigned int link_state);

#endif
