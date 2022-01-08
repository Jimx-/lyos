#include <string.h>
#include <lyos/list.h>
#include <lyos/sysutils.h>
#include <netinet/in.h>
#include <errno.h>

#include "inet.h"
#include "ifdev.h"

#define MAX_VTYPE 4
static struct {
    const char* name;
    size_t name_len;
    int (*create)(const char*);
} ifdev_vtype[MAX_VTYPE];
static unsigned int nr_vtypes;

static DEF_LIST(ifdev_list);

#define to_ifdev(netif) list_entry(netif, struct if_device, netif)

struct if_device* ifdev_find_by_name(const char* name)
{
    struct if_device* ifdev;

    list_for_each_entry(ifdev, &ifdev_list, list)
    {
        if (!strcmp(ifdev->name, name)) return ifdev;
    }

    return NULL;
}

static err_t ifdev_init_netif(struct netif* netif)
{
    struct if_device* ifdev = to_ifdev(netif);
    assert(ifdev);

    netif->mtu = ifdev->mtu;

    return ifdev->ifd_ops->ido_init(ifdev, netif);
}

void ifdev_poll(void)
{
    struct if_device* ifdev;

    list_for_each_entry(ifdev, &ifdev_list, list)
    {
        if (ifdev->ifd_ops->ido_poll) ifdev->ifd_ops->ido_poll(ifdev);
    }
}

void ifdev_update_hwaddr(struct if_device* ifdev, const u8* hwaddr)
{
    memcpy(ifdev->netif.hwaddr, hwaddr, ifdev->hwaddr_len);
}

void ifdev_add(struct if_device* ifdev, const char* name, unsigned int ifflags,
               unsigned int hwaddr_len, unsigned int mtu,
               const struct if_device_operations* ifd_ops)
{
    ip4_addr_t ip4_addr_any, ip4_addr_none;

    strlcpy(ifdev->name, name, sizeof(ifdev->name));
    ifdev->ifflags = 0;
    ifdev->hwaddr_len = hwaddr_len;
    ifdev->mtu = mtu;
    ifdev->ifd_ops = ifd_ops;

    list_add(&ifdev->list, &ifdev_list);

    ip4_addr_set_any(&ip4_addr_any);
    ip4_addr_set_u32(&ip4_addr_none, PP_HTONL(INADDR_NONE));

    if ((netif_add(&ifdev->netif, &ip4_addr_any, &ip4_addr_none, &ip4_addr_any,
                   ifdev, ifdev_init_netif, ifd_ops->ido_input)) == NULL)
        panic("inet: cannot add netif");

    ifdev_update_ifflags(ifdev, ifflags);
}

void ifdev_input(struct if_device* ifdev, struct pbuf* pbuf,
                 struct netif* netif)
{
    err_t err;

    ifdev->stats.rx_packets++;
    ifdev->stats.rx_bytes += pbuf->tot_len;

    if (netif) {

    } else {
        err = ifdev->netif.input(pbuf, &ifdev->netif);
    }

    if (err != ERR_OK) pbuf_free(pbuf);
}

void ifdev_register(const char* name, int (*create)(const char*))
{
    assert(nr_vtypes < MAX_VTYPE);

    ifdev_vtype[nr_vtypes].name = name;
    ifdev_vtype[nr_vtypes].name_len = strlen(name);
    ifdev_vtype[nr_vtypes].create = create;
    nr_vtypes++;
}

static int lookup_ifdev_vtype(const char* name, unsigned int* slot)
{
    int i;
    size_t name_len = strlen(name);

    for (i = 0; i < nr_vtypes; i++) {
        if (name_len >= ifdev_vtype[i].name_len &&
            !memcmp(name, ifdev_vtype[i].name, ifdev_vtype[i].name_len))
            break;
    }

    if (i == nr_vtypes) return EINVAL;

    if (slot) *slot = i;

    return 0;
}

int ifdev_create(const char* name)
{
    unsigned int slot;

    if (lookup_ifdev_vtype(name, &slot) != 0) return EINVAL;

    return ifdev_vtype[slot].create(name);
}

void ifdev_update_ifflags(struct if_device* ifdev, unsigned int ifflags)
{
    struct netif* netif = &ifdev->netif;

    ifdev->ifflags = ifflags;

    if ((ifflags & IFF_UP) && !netif_is_up(netif)) {
        netif_set_up(netif);
    } else if (!(ifflags & IFF_UP) && netif_is_up(netif)) {
        netif_set_down(netif);
    }
}

int ifdev_set_ifflags(struct if_device* ifdev, unsigned int ifflags)
{
    ifflags &= ~IFF_LOOPBACK;
    ifflags |= ifdev->ifflags & IFF_LOOPBACK;

    ifdev_update_ifflags(ifdev, ifflags);

    return 0;
}
