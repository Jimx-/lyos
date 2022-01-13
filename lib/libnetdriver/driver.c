#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/service.h>

#include <string.h>
#include <assert.h>

#include "libnetdriver.h"

#define SENDQ_MAX 8
#define RECVQ_MAX 8

static const struct netdriver* netdriver_tab;

static struct netdriver_data sendq[SENDQ_MAX];
static unsigned int pending_sends, sendq_tail;

static struct netdriver_data recvq[RECVQ_MAX];
static unsigned int pending_recvs, recvq_tail;

static netdriver_addr_t dev_hwaddr;
static int dev_link;

int netdriver_copyin(struct netdriver_data* data, void* buf, size_t size)
{
    return iov_grant_iter_copy_from(&data->iter, buf, size);
}

int netdriver_copyout(struct netdriver_data* data, const void* buf, size_t size)
{
    return iov_grant_iter_copy_to(&data->iter, buf, size);
}

static int netdriver_init(void)
{
    dev_link = NDEV_LINK_UNKNOWN;

    return netdriver_tab->ndr_init(0, &dev_hwaddr);
}

static void finish_send(int status)
{
    struct netdriver_data* data;
    MESSAGE msg;

    assert(pending_sends > 0);
    data = &sendq[sendq_tail];

    memset(&msg, 0, sizeof(msg));
    msg.type = NDEV_SEND_REPLY;
    msg.u.m_ndev_reply.id = data->id;
    msg.u.m_ndev_reply.status = status;

    asyncsend3(data->endpoint, &msg, 0);

    sendq_tail = (sendq_tail + 1) % SENDQ_MAX;
    pending_sends--;
}

void netdriver_send(void)
{
    struct netdriver_data* data;
    int retval;

    while (pending_sends) {
        data = &sendq[sendq_tail];

        retval = netdriver_tab->ndr_send(data, data->size);

        if (retval == SUSPEND) break;

        finish_send(retval);
    }
}

static void finish_recv(int status)
{
    struct netdriver_data* data;
    MESSAGE msg;

    assert(pending_recvs > 0);
    data = &recvq[recvq_tail];

    memset(&msg, 0, sizeof(msg));
    msg.type = NDEV_RECV_REPLY;
    msg.u.m_ndev_reply.id = data->id;
    msg.u.m_ndev_reply.status = status;

    asyncsend3(data->endpoint, &msg, 0);

    recvq_tail = (recvq_tail + 1) % RECVQ_MAX;
    pending_recvs--;
}

void netdriver_recv(void)
{
    struct netdriver_data* data;
    int retval;

    while (pending_recvs) {
        data = &recvq[recvq_tail];

        do {
            retval = netdriver_tab->ndr_recv(data, data->size);
        } while (retval == 0);

        if (retval == SUSPEND) break;

        finish_recv(retval);
    }
}

static void do_init(const struct netdriver* ndr, const MESSAGE* msg)
{
    MESSAGE reply_msg;

    pending_sends = pending_recvs = 0;

    memset(&reply_msg, 0, sizeof(reply_msg));
    reply_msg.type = NDEV_INIT_REPLY;
    reply_msg.u.m_ndev_init_reply.id = msg->u.m_ndev_init.id;
    memcpy(reply_msg.u.m_ndev_init_reply.hwaddr, dev_hwaddr.addr,
           sizeof(dev_hwaddr.addr));
    reply_msg.u.m_ndev_init_reply.hwaddr_len = sizeof(dev_hwaddr.addr);
    reply_msg.u.m_ndev_init_reply.max_send = SENDQ_MAX;
    reply_msg.u.m_ndev_init_reply.max_recv = RECVQ_MAX;
    reply_msg.u.m_ndev_init_reply.link = dev_link;

    asyncsend3(msg->source, &reply_msg, 0);
}

static void do_transfer(const struct netdriver* ndr, const MESSAGE* msg,
                        int do_send)
{
    struct netdriver_data* data;
    unsigned int count;
    int i;

    if (do_send) {
        assert(pending_sends < SENDQ_MAX);
        data = &sendq[(sendq_tail + pending_sends) % SENDQ_MAX];
    } else {
        assert(pending_recvs < RECVQ_MAX);
        data = &recvq[(recvq_tail + pending_recvs) % RECVQ_MAX];
    }

    data->endpoint = msg->source;
    data->id = msg->u.m_ndev_transfer.id;
    count = msg->u.m_ndev_transfer.count;
    data->size = 0;

    for (i = 0; i < count; i++) {
        data->iovec[i].iov_grant = msg->u.m_ndev_transfer.grant[i];
        data->iovec[i].iov_len = msg->u.m_ndev_transfer.len[i];
        data->size += data->iovec[i].iov_len;
    }

    iov_grant_iter_init(&data->iter, data->endpoint, data->iovec, count,
                        data->size);

    if (do_send)
        pending_sends++;
    else
        pending_recvs++;

    if (do_send)
        netdriver_send();
    else
        netdriver_recv();
}

void netdriver_process(const struct netdriver* ndr, MESSAGE* msg)
{
    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case INTERRUPT:
            if (ndr->ndr_intr) ndr->ndr_intr(msg->INTERRUPTS);
            break;
        default:
            break;
        }

        return;
    }

    switch (msg->type) {
    case NDEV_INIT:
        do_init(ndr, msg);
        break;
    case NDEV_SEND:
    case NDEV_RECV:
        do_transfer(ndr, msg, msg->type == NDEV_SEND);
        break;
    default:
        if (ndr->ndr_other) ndr->ndr_other(msg);
        break;
    }
}

void netdriver_task(const struct netdriver* ndr)
{
    MESSAGE msg;

    netdriver_tab = ndr;

    serv_register_init_fresh_callback(netdriver_init);
    serv_init();

    while (1) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);

        netdriver_process(ndr, &msg);
    }
}
