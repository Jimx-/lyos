#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>

#include <libsysfs/libsysfs.h>

struct ndev_queue {
    u32 head;
    u16 count;
    u16 max;
    struct list_head reqs;
};

struct ndev {
    struct list_head list;
    endpoint_t endpoint;
    struct ndev_queue sendq;
    struct ndev_queue recvq;
};

static DEF_LIST(ndevs);

static void ndev_queue_init(struct ndev_queue* queue)
{
    queue->head = 0;
    queue->count = 0;
    queue->max = 0;
    INIT_LIST_HEAD(&queue->reqs);
}

static struct ndev* lookup_ndev(endpoint_t endpoint)
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

    if ((retval = sysfs_subscribe("services\\.netdrv\\.[^.]*", SF_CHECK_NOW)) !=
        0)
        panic("inet: failed to subscribe to net driver events (%d)", retval);
}

static void ndev_send_init(struct ndev* ndev)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = NDEV_INIT;
    msg.u.m_ndev_init.id = ndev->sendq.head;

    asyncsend3(ndev->endpoint, &msg, 0);
}

static void ndev_up(const char* label, endpoint_t endpoint)
{
    struct ndev* ndev;

    ndev = lookup_ndev(endpoint);

    if (ndev == NULL) {
        ndev = malloc(sizeof(*ndev));
        if (!ndev) return;
        memset(ndev, 0, sizeof(*ndev));
    } else {
        list_del(&ndev->list);
    }

    ndev->endpoint = endpoint;
    ndev_queue_init(&ndev->sendq);
    ndev_queue_init(&ndev->recvq);

    ndev_send_init(ndev);

    list_add(&ndev->list, &ndevs);
}

void ndev_check(void)
{
    char label[PATH_MAX];
    char key[PATH_MAX];
    int type, event;
    endpoint_t endpoint;
    int retval;

    while (sysfs_get_event(label, &type, &event) == 0) {
        snprintf(key, sizeof(key), "%s.endpoint", label);

        if ((retval = sysfs_retrieve_u32(key, &endpoint)) != 0) {
            continue;
        }

        ndev_up(label, endpoint);
    }
}

void ndev_process(MESSAGE* msg)
{
    struct ndev* ndev;

    ndev = lookup_ndev(msg->source);
    if (!ndev) return;

    switch (msg->type) {
    case NDEV_INIT_REPLY:
        break;
    }
}
