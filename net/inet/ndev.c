#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <lyos/mgrant.h>
#include <sys/socket.h>

#include <libsysfs/libsysfs.h>

#include "inet.h"
#include "ethdev.h"

struct ndev_req {
    struct list_head list;
    int req_type;
    mgrant_id_t grant[NDEV_IOV_MAX];
};

struct ndev_queue {
    u32 head;
    u16 count;
    u16 max;
    struct list_head reqs;
};

struct ndev {
    unsigned int id;
    struct list_head list;
    endpoint_t endpoint;
    struct eth_device* ethdev;
    struct ndev_queue sendq;
    struct ndev_queue recvq;
};

static DEF_LIST(ndevs);

static struct idr ndev_idr;

static void ndev_down(struct ndev* ndev);

static void ndev_queue_init(struct ndev_queue* nq)
{
    nq->head = 0;
    nq->count = 0;
    nq->max = 0;
    INIT_LIST_HEAD(&nq->reqs);
}

static void ndev_queue_add(struct ndev_queue* nq, struct ndev_req* req)
{
    list_add_tail(&req->list, &nq->reqs);
    nq->count++;
}

static void ndev_queue_advance(struct ndev_queue* nq)
{
    struct ndev_req* req;
    int i;

    req = list_first_entry(&nq->reqs, struct ndev_req, list);

    for (i = 0; i < NDEV_IOV_MAX; i++) {
        mgrant_id_t grant = req->grant[i];

        if (grant == GRANT_INVALID) break;

        mgrant_revoke(grant);
    }

    list_del(&req->list);
    nq->head++;
    nq->count--;
    free(req);
}

static int ndev_queue_remove(struct ndev_queue* nq, int type, unsigned int seq)
{
    struct ndev_req* req;

    if (nq->count == 0 || nq->head != seq) return FALSE;

    assert(!list_empty(&nq->reqs));
    req = list_first_entry(&nq->reqs, struct ndev_req, list);
    if (req->req_type != type) return FALSE;

    ndev_queue_advance(nq);
    return TRUE;
}

static struct ndev* lookup_ndev(unsigned int id)
{
    struct ndev* ndev;

    list_for_each_entry(ndev, &ndevs, list)
    {
        if (ndev->id == id) return ndev;
    }

    return NULL;
}

static struct ndev* lookup_ndev_endpoint(endpoint_t endpoint)
{
    struct ndev* ndev;

    list_for_each_entry(ndev, &ndevs, list)
    {
        if (ndev->endpoint == endpoint) return ndev;
    }

    return NULL;
}

void ndev_init(void)
{
    int retval;

    idr_init(&ndev_idr);

    if ((retval = sysfs_subscribe("services\\.netdrv\\.[^.]*\\.endpoint",
                                  SF_CHECK_NOW)) != 0)
        panic("inet: failed to subscribe to net driver events (%d)", retval);
}

static void ndev_send_init(struct ndev* ndev)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = NDEV_INIT;
    msg.u.m_ndev_init.id = ndev->sendq.head;

    if (asyncsend3(ndev->endpoint, &msg, 0) != 0)
        panic("inet: failed to send ndev init request");
}

static void ndev_up(const char* label, endpoint_t endpoint)
{
    struct ndev* ndev;

    ndev = lookup_ndev_endpoint(endpoint);

    if (ndev == NULL) {
        ndev = malloc(sizeof(*ndev));
        if (!ndev) return;
        memset(ndev, 0, sizeof(*ndev));
    } else {
        list_del(&ndev->list);
    }

    ndev->id = idr_alloc(&ndev_idr, ndev, 1, 0);
    ndev->endpoint = endpoint;
    ndev_queue_init(&ndev->sendq);
    ndev_queue_init(&ndev->recvq);

    ndev_send_init(ndev);

    list_add(&ndev->list, &ndevs);
}

static void ndev_down(struct ndev* ndev) {}

void ndev_check(void)
{
    char label[PATH_MAX];
    int type, event;
    endpoint_t endpoint;
    int retval;

    while (sysfs_get_event(label, &type, &event) == 0) {
        if ((retval = sysfs_retrieve_u32(label, (u32*)&endpoint)) != 0) {
            continue;
        }

        /* Skip the ".endpoint" part */
        label[strlen(label) - 9] = '\0';
        ndev_up(label, endpoint);
    }
}

static void ndev_init_reply(struct ndev* ndev, const MESSAGE* msg)
{
    struct ndev_hwaddr hwaddr;
    int hwaddr_len;
    u8 max_send, max_recv;
    int link;
    int enabled = FALSE;

    if (msg->u.m_ndev_init_reply.id != ndev->sendq.head) return;

    hwaddr_len = msg->u.m_ndev_init_reply.hwaddr_len;
    if (hwaddr_len < 1 || hwaddr_len > NDEV_HWADDR_MAX) {
        ndev_down(ndev);
        return;
    }

    max_send = msg->u.m_ndev_init_reply.max_send;
    max_recv = msg->u.m_ndev_init_reply.max_recv;
    link = msg->u.m_ndev_init_reply.link;

    if (ndev->ethdev == NULL) {
        ndev->ethdev = ethdev_add(ndev->id);
    }

    if (ndev->ethdev) {
        ndev->sendq.max = max_send;
        ndev->sendq.head++;
        ndev->recvq.max = max_recv;
        ndev->recvq.head++;

        memset(hwaddr.hwaddr, 0, sizeof(hwaddr.hwaddr));
        memcpy(hwaddr.hwaddr, msg->u.m_ndev_init_reply.hwaddr, hwaddr_len);

        enabled = ethdev_enable(ndev->ethdev, &hwaddr, hwaddr_len, link);
    }

    if (!enabled) ndev_down(ndev);
}

static struct ndev_req* ndev_queue_get(struct ndev_queue* nq, int type,
                                       unsigned int* seq)
{
    struct ndev_req* req;

    if (nq->count == nq->max) return NULL;

    req = malloc(sizeof(*req));
    if (!req) return NULL;

    memset(req, 0, sizeof(*req));
    req->req_type = type;

    *seq = nq->head + nq->count;

    return req;
}

static int ndev_transfer(struct ndev* ndev, const struct pbuf* pbuf,
                         int do_send, unsigned int seq, struct ndev_req* req)
{
    size_t len = pbuf->tot_len;
    MESSAGE msg;
    int i;
    int retval;

    memset(&msg, 0, sizeof(msg));
    msg.type = do_send ? NDEV_SEND : NDEV_RECV;
    msg.u.m_ndev_transfer.id = seq;

    for (i = 0; len; i++) {
        mgrant_id_t grant;

        assert(i < NDEV_IOV_MAX);

        grant = mgrant_set_direct(ndev->endpoint, pbuf->payload, pbuf->len,
                                  do_send ? MGF_READ : MGF_WRITE);
        if (grant == GRANT_INVALID) {
            while (i--)
                mgrant_revoke(req->grant[i]);

            return ENOMEM;
        }

        msg.u.m_ndev_transfer.grant[i] = grant;
        msg.u.m_ndev_transfer.len[i] = pbuf->len;

        req->grant[i] = grant;

        assert(len >= pbuf->len);
        len -= pbuf->len;
        pbuf = pbuf->next;
    }

    msg.u.m_ndev_transfer.count = i;

    for (; i < NDEV_IOV_MAX; i++)
        req->grant[i] = GRANT_INVALID;

    if ((retval = asyncsend3(ndev->endpoint, &msg, 0)) != 0)
        panic("inet: failed to send ndev transfer (%d)", retval);

    return 0;
}

int ndev_can_recv(unsigned int id)
{
    struct ndev* ndev;

    ndev = lookup_ndev(id);
    assert(ndev);

    return ndev->recvq.count < ndev->recvq.max;
}

int ndev_send(unsigned int id, struct pbuf* pbuf)
{
    struct ndev* ndev;
    struct ndev_req* req;
    unsigned int seq;
    int retval;

    ndev = lookup_ndev(id);
    assert(ndev);

    if ((req = ndev_queue_get(&ndev->sendq, NDEV_SEND, &seq)) == NULL)
        return EBUSY;

    if ((retval = ndev_transfer(ndev, pbuf, TRUE, seq, req)) != 0) {
        free(req);
        return retval;
    }

    ndev_queue_add(&ndev->sendq, req);
    return 0;
}

int ndev_recv(unsigned int id, struct pbuf* pbuf)
{
    struct ndev* ndev;
    struct ndev_req* req;
    unsigned int seq;
    int retval;

    ndev = lookup_ndev(id);
    assert(ndev);

    if ((req = ndev_queue_get(&ndev->recvq, NDEV_RECV, &seq)) == NULL)
        return EBUSY;

    if ((retval = ndev_transfer(ndev, pbuf, FALSE, seq, req)) != 0) {
        free(req);
        return retval;
    }

    ndev_queue_add(&ndev->recvq, req);
    return 0;
}

static void ndev_send_reply(struct ndev* ndev, const MESSAGE* msg)
{
    if (!ndev_queue_remove(&ndev->sendq, NDEV_SEND, msg->u.m_ndev_reply.id))
        return;

    ethdev_sent(ndev->ethdev, msg->u.m_ndev_reply.status);
}

static void ndev_recv_reply(struct ndev* ndev, const MESSAGE* msg)
{
    if (!ndev_queue_remove(&ndev->recvq, NDEV_RECV, msg->u.m_ndev_reply.id))
        return;

    ethdev_recv(ndev->ethdev, msg->u.m_ndev_reply.status);
}

void ndev_process(MESSAGE* msg)
{
    struct ndev* ndev;

    ndev = lookup_ndev_endpoint(msg->source);
    if (!ndev) return;

    switch (msg->type) {
    case NDEV_INIT_REPLY:
        ndev_init_reply(ndev, msg);
        break;

    case NDEV_SEND_REPLY:
        ndev_send_reply(ndev, msg);
        break;

    case NDEV_RECV_REPLY:
        ndev_recv_reply(ndev, msg);
        break;
    }
}
