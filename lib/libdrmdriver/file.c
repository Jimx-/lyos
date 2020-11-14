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
#include <asm/ioctl.h>
#include <drm/drm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/eventpoll.h>

#include <libdevman/libdevman.h>

#include "libdrmdriver.h"
#include "proto.h"

int drm_event_reserve(struct drm_device* dev, struct drm_pending_event* p,
                      struct drm_event* e)
{
    if (dev->event_space < e->length) return ENOMEM;

    dev->event_space -= e->length;

    p->event = e;
    list_add(&p->pending_link, &dev->pending_event_list);
    p->dev = dev;

    return 0;
}

static void drm_in_transfer(struct drm_device* dev)
{
    ssize_t retval = 0;
    size_t length;

    for (;;) {
        struct drm_pending_event* e = NULL;

        if (!list_empty(&dev->event_list))
            e = list_first_entry(&dev->event_list, struct drm_pending_event,
                                 link);
        else
            break;

        length = e->event->length;

        if (length > dev->inleft - retval) break;

        if (safecopy_to(dev->incaller, dev->ingrant, retval, e->event,
                        length) != OK) {
            if (retval == 0) retval = -EFAULT;
            break;
        }

        dev->event_space += length;
        list_del(&e->link);

        retval += length;
        free(e);
    }

    if (retval != 0) {
        chardriver_reply_io(dev->incaller, dev->inid, retval);
        dev->inleft = dev->incnt = 0;
        dev->incaller = NO_TASK;
    }
}

void drm_send_event(struct drm_device* dev, struct drm_pending_event* e)
{
    if (!e->dev) {
        free(e);
        return;
    }

    list_del(&e->pending_link);
    list_add_tail(&e->link, &dev->event_list);

    drm_in_transfer(dev);

    drm_select_retry(dev);
}

ssize_t drm_do_read(struct drm_device* dev, u64 pos, endpoint_t endpoint,
                    mgrant_id_t grant, unsigned int count, int flags,
                    cdev_id_t id)
{
    if (dev->incaller != NO_TASK || dev->inleft > 0) return -EIO;
    if (count <= 0) return -EINVAL;

    dev->incaller = endpoint;
    dev->inid = id;
    dev->ingrant = grant;
    dev->inleft = count;
    dev->incnt = 0;

    drm_in_transfer(dev);

    if (dev->inleft > 0 && (flags & CDEV_NONBLOCK)) {
        dev->inleft = dev->incnt = 0;
        dev->incaller = NO_TASK;
        return -EAGAIN;
    }

    return SUSPEND;
}

__poll_t drm_select_try(struct drm_device* dev)
{
    __poll_t mask = 0;

    if (!list_empty(&dev->event_list)) mask |= EPOLLIN | EPOLLRDNORM;

    return mask;
}
