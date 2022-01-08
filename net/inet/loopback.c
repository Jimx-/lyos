#include <lyos/list.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "inet.h"
#include "ifdev.h"

#define LOOPIF_MAX_MTU (UINT16_MAX - sizeof(uint32_t))
#define LOOPIF_DEF_MTU LOOPIF_MAX_MTU

struct loopif {
    struct list_head list;
    struct if_device ifdev;
    struct pbuf* head;
    struct pbuf** tailp;
};

#define to_loopif(ifdev) list_entry(ifdev, struct loopif, ifdev)

static DEF_LIST(loopif_list);

static err_t loopif_init_netif(struct if_device* ifdev, struct netif* netif);
static void loopif_poll(struct if_device* ifdev);

static const struct if_device_operations loopif_ifd_ops = {
    .ido_init = loopif_init_netif,
    .ido_input = ip_input,
    .ido_poll = loopif_poll,
};

static int loopif_create(const char* name)
{
    struct loopif* loopif;

    loopif = malloc(sizeof(*loopif));
    if (!loopif) return ENOMEM;

    memset(loopif, 0, sizeof(*loopif));
    list_add(&loopif->list, &loopif_list);

    loopif->head = NULL;
    loopif->tailp = &loopif->head;

    ifdev_add(&loopif->ifdev, name, IFF_LOOPBACK | IFF_MULTICAST, 0,
              LOOPIF_DEF_MTU, &loopif_ifd_ops);
    return 0;
}

void loopif_init(void) { ifdev_register("lo", loopif_create); }

static err_t loopif_init_netif(struct if_device* ifdev, struct netif* netif)
{
    netif->name[0] = 'l';
    netif->name[1] = 'o';

    netif->flags |= NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;

    return ERR_OK;
}

static void loopif_poll(struct if_device* ifdev) {}
