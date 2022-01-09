#include <lyos/list.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <lyos/sysutils.h>

#include "inet.h"
#include "ifdev.h"

#define LOOPIF_MAX_MTU (UINT16_MAX - sizeof(uint32_t))
#define LOOPIF_DEF_MTU LOOPIF_MAX_MTU

#define LOOPIF_LIMIT 65536

struct loopif {
    struct list_head list;
    struct if_device ifdev;
    struct pbuf* head;
    struct pbuf** tailp;
};

#define to_loopif(ifdev) list_entry(ifdev, struct loopif, ifdev)

static DEF_LIST(loopif_list);

static err_t loopif_init_netif(struct if_device* ifdev, struct netif* netif);
static err_t loopif_output(struct if_device* ifdev, struct pbuf* pbuf,
                           struct netif* netif);
static void loopif_poll(struct if_device* ifdev);

static const struct if_device_operations loopif_ifd_ops = {
    .ido_init = loopif_init_netif,
    .ido_input = ip_input,
    .ido_output = loopif_output,
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

    ifdev_update_link(&loopif->ifdev, LINK_STATE_UP);

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

static err_t loopif_output(struct if_device* ifdev, struct pbuf* pbuf,
                           struct netif* netif)
{
    struct loopif* loopif = to_loopif(ifdev);
    struct pbuf* pcopy;
    struct if_device* oifdev;
    unsigned int oifidx = 0;

    assert(pbuf->tot_len <= UINT16_MAX - sizeof(oifidx));

    pcopy = pbuf_alloc(PBUF_RAW, sizeof(oifidx) + pbuf->tot_len, PBUF_RAM);
    if (!pcopy) {
        ifdev_output_drop(ifdev);
        return ERR_MEM;
    }

    if (netif) {
        oifdev = netif_get_ifdev(netif);
        oifidx = ifdev_get_index(oifdev);
    }

    memcpy(pcopy->payload, &oifidx, sizeof(oifidx));
    pbuf_header(pcopy, -(long)sizeof(oifidx));

    if (pbuf_copy(pcopy, pbuf) != ERR_OK) panic("inet: failed to copy pbuf");

    pbuf_header(pcopy, sizeof(oifidx));

    *loopif->tailp = pcopy;
    while (pcopy->tot_len != pcopy->len)
        pcopy = pcopy->next;
    loopif->tailp = &pcopy->next;

    return ERR_OK;
}

static void loopif_poll(struct if_device* ifdev)
{
    struct loopif* loopif = to_loopif(ifdev);
    struct pbuf *pbuf, *pend;
    struct netif* netif = NULL;
    struct if_device* oifdev;
    unsigned int count = 0;
    unsigned int oifidx;

    while ((pbuf = loopif->head) != NULL) {
        if (count++ >= LOOPIF_LIMIT) break;

        pend = pbuf;
        while (pend->tot_len != pend->len)
            pend = pend->next;

        loopif->head = pend->next;
        if (!loopif->head) loopif->tailp = &loopif->head;
        pend->next = NULL;

        memcpy(&oifidx, pbuf->payload, sizeof(oifidx));
        pbuf_header(pbuf, -(int)sizeof(oifidx));

        if (oifidx) {
            oifdev = ifdev_find_by_index(oifidx);
            if (oifdev) netif = &oifdev->netif;
        }

        ifdev_input(ifdev, pbuf, netif);
    }
}
