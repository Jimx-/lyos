#include <lyos/config.h>
#include <lyos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/list.h>
#include <net/if_arp.h>

#include "inet.h"
#include "ethdev.h"
#include "ifdev.h"

#define ETHARP_HWADDR_LEN 6

#define ETH_PAD_LEN 2
#define ETH_HDR_LEN 14

#define ETH_MAX_MTU 1500
#define ETH_DEF_MTU ETH_MAX_MTU

struct eth_device {
    struct list_head list;
    struct if_device ifdev;
    unsigned int id;

    struct {
        struct pbuf* head;
        struct pbuf** tailp;
    } recvq;
};

#define to_eth_device(ifdev) list_entry(ifdev, struct eth_device, ifdev)

static err_t ethdev_init_netif(struct if_device* ifdev, struct netif* netif);
static void ethdev_poll(struct if_device* ifdev);

static const struct if_device_operations ethdev_ifd_ops = {
    .ido_init = ethdev_init_netif,
    .ido_input = netif_input,
    .ido_poll = ethdev_poll,
};

static err_t ethdev_init_netif(struct if_device* ifdev, struct netif* netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';

    memset(netif->hwaddr, 0, sizeof(netif->hwaddr));

    netif->flags = NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

    return ERR_OK;
}

struct eth_device* ethdev_add(unsigned int id)
{
    struct eth_device* ethdev;
    char name[IFNAMSIZ];
    unsigned int ifflags;

    ethdev = malloc(sizeof(*ethdev));
    if (!ethdev) return NULL;

    memset(ethdev, 0, sizeof(*ethdev));
    ethdev->id = id;
    snprintf(name, sizeof(name), "eth%u", id);

    ethdev->recvq.head = NULL;
    ethdev->recvq.tailp = &ethdev->recvq.head;

    ifflags = 0;

    ifdev_add(&ethdev->ifdev, name, ifflags, ARPHRD_ETHER, ETHARP_HWADDR_LEN,
              ETH_DEF_MTU, &ethdev_ifd_ops);

    return ethdev;
}

static int ethdev_can_recv(struct eth_device* ethdev) { return TRUE; }

void ethdev_recv(struct eth_device* ethdev, int status)
{
    struct pbuf *pbuf, *pend;
    size_t len;

    pbuf = ethdev->recvq.head;
    assert(pbuf);

    pend = pbuf;
    while (pend->tot_len != pend->len)
        pend = pend->next;

    ethdev->recvq.head = pend->next;
    if (!ethdev->recvq.head) ethdev->recvq.tailp = &ethdev->recvq.head;
    pend->next = NULL;

    if (status < 0 || !ethdev_can_recv(ethdev)) {
        pbuf_free(pbuf);
        return;
    }

    if (status > pbuf->tot_len) {
        printl("inet: bad receive packet len %d\n", status);
        return;
    }

    len = (size_t)status;

    for (pend = pbuf;; pend = pend->next) {
        pend->tot_len = len;
        if (pend->len > len) pend->len = len;
        len -= pend->len;
        if (!len) break;
    }

    if (pend->next) {
        pbuf_free(pend->next);
        pend->next = NULL;
    }

    ifdev_input(&ethdev->ifdev, pbuf, NULL);
}

int ethdev_enable(struct eth_device* ethdev, const struct ndev_hwaddr* hwaddr,
                  int hwaddr_len)
{
    ifdev_update_hwaddr(&ethdev->ifdev, hwaddr->hwaddr);

    return TRUE;
}

static void ethdev_poll(struct if_device* ifdev)
{
    struct eth_device* ethdev = to_eth_device(ifdev);
    struct pbuf* pbuf;

    while (ethdev_can_recv(ethdev) && ndev_can_recv(ethdev->id)) {
        pbuf = pbuf_alloc(PBUF_RAW, ETH_PAD_LEN + ETH_HDR_LEN + ETH_MAX_MTU,
                          PBUF_RAM);
        if (!pbuf) break;

        pbuf_header(pbuf, -ETH_PAD_LEN);

        if (ndev_recv(ethdev->id, pbuf) != 0) {
            pbuf_free(pbuf);
            break;
        }

        *ethdev->recvq.tailp = pbuf;
        while (pbuf->tot_len != pbuf->len)
            pbuf = pbuf->next;
        ethdev->recvq.tailp = &pbuf->next;
    }
}
