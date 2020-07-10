#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/service.h>
#include <lyos/input.h>

#include <libsysfs/libsysfs.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "input.h"

#define EVDEV_MINOR_BASE 32
#define EVDEV_MINORS 32

#define EVDEV_MIN_BUFFER_SIZE 64 /* must be power of 2 */

struct evdev_client {
    size_t head;
    size_t packet_head;
    size_t tail;
    struct input_event buffer[EVDEV_MIN_BUFFER_SIZE];
    size_t bufsize;

    int suspended;
    endpoint_t caller;
    void* req_buf;
    size_t req_size;
    cdev_id_t req_id;
};

struct evdev {
    int open;
    struct input_handle handle;
    device_id_t dev_id;

    struct evdev_client client;
};

static int evdev_get_new_minor(void)
{
    static int minor = EVDEV_MINOR_BASE;

    if (minor >= EVDEV_MINOR_BASE + EVDEV_MINORS) {
        return -ENOSPC;
    }

    return minor++;
}

static ssize_t evdev_copy_events(struct evdev_client* client,
                                 endpoint_t endpoint, char* buf, size_t size)
{
    size_t event_size = sizeof(struct input_event);
    size_t event_count, nbytes = 0, copy_count = 0;
    size_t tail;

    event_count = size / event_size;
    assert(event_count > 0);

    tail = client->tail;
    while (tail != client->packet_head && tail < client->bufsize &&
           copy_count < event_count) {
        copy_count++;
        tail = (tail + 1) & (client->bufsize - 1);
    }

    data_copy(endpoint, buf, SELF, &client->buffer[client->tail],
              copy_count * event_size);
    buf += copy_count * event_size;
    nbytes += copy_count * event_size;
    event_count -= copy_count;

    if (tail != client->packet_head && event_count > 0) {
        /* copy the left part */
        tail = 0;
        copy_count = 0;

        while (tail != client->packet_head && tail < client->bufsize &&
               copy_count < event_count) {
            copy_count++;
            tail = (tail + 1) & (client->bufsize - 1);
        }

        data_copy(endpoint, buf, SELF, client->buffer, copy_count * event_size);
        nbytes += copy_count * event_size;
    }

    client->tail = tail;

    return nbytes;
}

static int evdev_open(struct input_handle* handle)
{
    struct evdev* evdev = (struct evdev*)handle->private;
    int retval;

    if (evdev->open) {
        return EBUSY;
    }

    if (!evdev->open++) {
        retval = input_open_device(handle);

        if (retval) {
            evdev->open--;
            return retval;
        }
    }

    return 0;
}

static int evdev_close(struct input_handle* handle)
{
    struct evdev* evdev = (struct evdev*)handle->private;

    if (!--evdev->open) {
        input_close_device(handle);
    }

    return 0;
}

static ssize_t evdev_read(struct input_handle* handle, endpoint_t endpoint,
                          char* buf, unsigned int count, cdev_id_t id)
{
    struct evdev* evdev = (struct evdev*)handle->private;
    struct evdev_client* client = &evdev->client;
    size_t event_size = sizeof(struct input_event);

    if (client->suspended) {
        return -EIO;
    }

    if (count != 0 && count < event_size) {
        return -EINVAL;
    }

    if (client->packet_head == client->tail) {
        /* no event */
        client->suspended = TRUE;
        client->caller = endpoint;
        client->req_buf = buf;
        client->req_size = count;
        client->req_id = id;

        return SUSPEND;
    }

    return evdev_copy_events(client, endpoint, buf, count);
}

static struct input_handle_ops evdev_handle_ops = {
    .open = evdev_open,
    .close = evdev_close,
    .read = evdev_read,
};

static void __pass_event(struct evdev_client* client,
                         const struct input_event* event)
{
    client->buffer[client->head++] = *event;
    client->head &= client->bufsize - 1;

    if (client->head == client->tail) {
        client->tail = (client->head - 2) & (client->bufsize - 1);

        client->buffer[client->tail].time = event->time;
        client->buffer[client->tail].type = EV_SYN;
        client->buffer[client->tail].code = SYN_DROPPED;
        client->buffer[client->tail].value = 0;
    }

    if (event->type == EV_SYN && event->code == SYN_REPORT) {
        client->packet_head = client->head;
    }
}

void evdev_events(struct input_handle* handle, const struct input_value* vals,
                  size_t count)
{
    struct evdev* evdev = (struct evdev*)handle->private;
    struct evdev_client* client = &evdev->client;
    struct input_event event;
    struct timeval tv;
    const struct input_value* val;
    ssize_t retval;
    int wakeup = FALSE;

    gettimeofday(&tv, NULL);
    event.input_event_sec = tv.tv_sec;
    event.input_event_usec = tv.tv_usec;

    for (val = vals; val != vals + count; val++) {
        if (val->type == EV_SYN && val->code == SYN_REPORT) {
            wakeup = TRUE;
        }

        event.type = val->type;
        event.code = val->code;
        event.value = val->value;
        __pass_event(&evdev->client, &event);
    }

    if (wakeup) {
        if (client->suspended) {
            /* wake up suspended request */
            retval = evdev_copy_events(client, client->caller, client->req_buf,
                                       client->req_size);
            chardriver_reply_io(TASK_FS, client->req_id, retval);

            client->suspended = FALSE;
            client->caller = NO_TASK;
            client->req_buf = NULL;
            client->req_size = 0;
        }
    }
}

void evdev_event(struct input_handle* handle, unsigned int type,
                 unsigned int code, int value)
{
    struct input_value val = {.type = type, .code = code, .value = value};

    evdev_events(handle, &val, 1);
}

static int evdev_connect(struct input_handler* handler, struct input_dev* dev)
{
    int minor;
    int dev_no;
    int retval;
    struct device_info devinf;
    struct evdev* evdev;
    dev_t devt;

    minor = evdev_get_new_minor();
    if (minor < 0) {
        return -minor;
    }

    evdev = malloc(sizeof(*evdev));
    if (!evdev) {
        retval = ENOMEM;
        goto err;
    }

    memset(evdev, 0, sizeof(*evdev));

    dev_no = minor - EVDEV_MINOR_BASE;

    evdev->handle.dev = dev;
    evdev->handle.handler = handler;
    evdev->handle.minor = minor;
    evdev->handle.ops = &evdev_handle_ops;
    evdev->handle.private = evdev;

    evdev->client.bufsize = EVDEV_MIN_BUFFER_SIZE;
    evdev->client.caller = NO_TASK;

    devt = MAKE_DEV(DEV_INPUT, minor);
    retval = dm_cdev_add(devt);
    if (retval) {
        goto err_free_evdev;
    }

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "event%d", dev_no);
    devinf.bus = NO_BUS_ID;
    devinf.class = input_class;
    devinf.parent = dev->input_dev_id;
    devinf.devt = MAKE_DEV(DEV_INPUT, minor);

    retval = dm_device_register(&devinf, &evdev->dev_id);
    if (retval) {
        goto err_free_evdev;
    }

    retval = input_register_handle(&evdev->handle);
    if (retval) {
        goto err_free_evdev;
    }

    return 0;

err_free_evdev:
    free(evdev);
err:
    return retval;
}

static struct input_handler evdev_handler = {
    .name = "evdev",
    .minor = EVDEV_MINOR_BASE,

    .event = evdev_event,
    .events = evdev_events,
    .connect = evdev_connect,
};

int evdev_init(void) { return input_register_handler(&evdev_handler); }
