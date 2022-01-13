#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/list.h>
#include <net/if_arp.h>
#include <lyos/sysutils.h>

#include <libnetdriver/libnetdriver.h>

#include <lwip/etharp.h>

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
        struct pbuf** unsentp;
        unsigned int count;
    } sendq;

    struct {
        struct pbuf* head;
        struct pbuf** tailp;
    } recvq;
};

#define to_eth_device(ifdev) list_entry(ifdev, struct eth_device, ifdev)

static err_t ethdev_init_netif(struct if_device* ifdev, struct netif* netif);
static err_t ethdev_output(struct if_device* ifdev, struct pbuf* pbuf,
                           struct netif* netif);
static err_t ethdev_linkoutput(struct netif* netif, struct pbuf* pbuf);
static void ethdev_poll(struct if_device* ifdev);

static const struct if_device_operations ethdev_ifd_ops = {
    .ido_init = ethdev_init_netif,
    .ido_input = netif_input,
    .ido_output = ethdev_output,
    .ido_output_v4 = etharp_output,
    .ido_poll = ethdev_poll,
};

static err_t ethdev_init_netif(struct if_device* ifdev, struct netif* netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';

    netif->linkoutput = ethdev_linkoutput;

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

    ethdev->sendq.head = NULL;
    ethdev->sendq.tailp = &ethdev->sendq.head;
    ethdev->sendq.unsentp = &ethdev->sendq.head;
    ethdev->sendq.count = 0;

    ethdev->recvq.head = NULL;
    ethdev->recvq.tailp = &ethdev->recvq.head;

    ifflags = 0;

    ifdev_add(&ethdev->ifdev, name, ifflags, ARPHRD_ETHER, ETHARP_HWADDR_LEN,
              ETH_DEF_MTU, &ethdev_ifd_ops);

    return ethdev;
}

static int ethdev_can_send(struct eth_device* ethdev)
{
    if (*ethdev->sendq.unsentp == NULL) return FALSE;

    return TRUE;
}

static void ethdev_dequeue_send(struct eth_device* ethdev)
{
    struct pbuf *pref, *pbuf, *pend, **nextp;
    unsigned int count;

    pref = ethdev->sendq.head;
    assert(pref);

    pend = pref;
    while (pend->tot_len > pend->len)
        pend = pend->next;

    nextp = &pend->next;

    if (ethdev->sendq.unsentp == nextp)
        ethdev->sendq.unsentp = &ethdev->sendq.head;

    ethdev->sendq.head = *nextp;
    if (!ethdev->sendq.head) ethdev->sendq.tailp = &ethdev->sendq.head;

    *nextp = NULL;

    if (PBUF_NEEDS_COPY(pref)) {
        pbuf = (struct pbuf*)pref->payload;
        pbuf_free(pbuf);
        count = pref->len;
    } else {
        count = pbuf_clen(pref);
    }

    assert(count);
    assert(ethdev->sendq.count >= count);
    ethdev->sendq.count -= count;

    pbuf_free(pref);
}

void ethdev_sent(struct eth_device* ethdev, int status)
{
    ethdev_dequeue_send(ethdev);

    if (status < 0) ifdev_output_drop(&ethdev->ifdev);
}

static int ethdev_can_recv(struct eth_device* ethdev)
{
    return ifdev_is_up(&ethdev->ifdev);
}

void ethdev_recv(struct eth_device* ethdev, int status)
{
    struct pbuf *pbuf, *pend;
    size_t len;

    pbuf = ethdev->recvq.head;
    assert(pbuf);

    pend = pbuf;
    while (pend->tot_len > pend->len)
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

static void ethdev_set_status(struct eth_device* ethdev, int link)
{
    unsigned int iflink;

    switch (link) {
    case NDEV_LINK_UP:
        iflink = LINK_STATE_UP;
        break;
    case NDEV_LINK_DOWN:
        iflink = LINK_STATE_DOWN;
        break;
    default:
        iflink = LINK_STATE_UNKNOWN;
        break;
    }

    ifdev_update_link(&ethdev->ifdev, iflink);
}

int ethdev_enable(struct eth_device* ethdev, const struct ndev_hwaddr* hwaddr,
                  int hwaddr_len, int link)
{
    ifdev_update_hwaddr(&ethdev->ifdev, hwaddr->hwaddr);

    ethdev_set_status(ethdev, link);

    return TRUE;
}

static void ethdev_poll(struct if_device* ifdev)
{
    struct eth_device* ethdev = to_eth_device(ifdev);
    struct pbuf *pbuf, *pref;

    while (ethdev_can_send(ethdev)) {
        pref = *ethdev->sendq.unsentp;

        if (PBUF_NEEDS_COPY(pref))
            pbuf = (struct pbuf*)pref->payload;
        else
            pbuf = pref;

        if (ndev_send(ethdev->id, pbuf) != 0) break;

        while (pref->tot_len > pref->len)
            pref = pref->next;
        ethdev->sendq.unsentp = &pref->next;
    }

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
        while (pbuf->tot_len > pbuf->len)
            pbuf = pbuf->next;
        ethdev->recvq.tailp = &pbuf->next;
    }
}

static err_t ethdev_output(struct if_device* ifdev, struct pbuf* pbuf,
                           struct netif* netif)
{
    struct eth_device* ethdev = to_eth_device(ifdev);
    struct pbuf *pcopy = NULL, *pref;
    size_t padding = 0;
    unsigned int count;
    err_t err;

    assert(pbuf->tot_len <= ETH_PACKET_MAX);

    if (pbuf->tot_len < ETH_PACKET_MIN)
        padding = ETH_PACKET_MIN - pbuf->tot_len;

    count = pbuf_clen(pbuf);

    if (padding || count > NDEV_IOV_MAX) {
        /* Coalesce send buffers. */
        pcopy = pbuf_alloc(PBUF_RAW, pbuf->tot_len + padding, PBUF_RAM);
        if (!pcopy) {
            ifdev_output_drop(ifdev);
            return ERR_MEM;
        }

        err = pbuf_copy(pcopy, pbuf);
        assert(err == ERR_OK);

        if (padding) memset((char*)pcopy->payload + pbuf->tot_len, 0, padding);

        pbuf = pcopy;
    }

    if (!pcopy) {
        pref = pbuf_alloc(PBUF_RAW, 0, PBUF_REF);
        if (!pref) {
            ifdev_output_drop(ifdev);
            return ERR_MEM;
        }

        pbuf_ref(pbuf);

        pref->payload = pbuf;
        pref->tot_len = 0;
        pref->len = count;
    } else
        pref = pcopy;

    *ethdev->sendq.tailp = pref;
    while (pref->tot_len > pref->len)
        pref = pref->next;
    ethdev->sendq.tailp = &pref->next;

    ethdev->sendq.count += count;

    return ERR_OK;
}

static err_t ethdev_linkoutput(struct netif* netif, struct pbuf* pbuf)
{
    struct if_device* ifdev = netif_get_ifdev(netif);
    return ifdev_output(ifdev, pbuf, NULL);
}
