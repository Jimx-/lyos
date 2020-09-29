#include <lyos/types.h>
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
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>

#include <libsysfs/libsysfs.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "input.h"

#define EVDEV_MINOR_BASE 32
#define EVDEV_MINORS     32

#define EVDEV_MIN_BUFFER_SIZE 64 /* must be power of 2 */

struct evdev_client {
    size_t head;
    size_t packet_head;
    size_t tail;
    struct input_event buffer[EVDEV_MIN_BUFFER_SIZE];
    size_t bufsize;

    int suspended;
    endpoint_t caller;
    mgrant_id_t req_grant;
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
                                 endpoint_t endpoint, mgrant_id_t grant,
                                 size_t size)
{
    size_t event_size = sizeof(struct input_event);
    size_t event_count, nbytes = 0, copy_count = 0;
    off_t copy_offset = 0;
    size_t tail;
    int retval;

    event_count = size / event_size;
    assert(event_count > 0);

    tail = client->tail;
    while (tail != client->packet_head && tail < client->bufsize &&
           copy_count < event_count) {
        copy_count++;
        tail = (tail + 1) & (client->bufsize - 1);
    }

    if ((retval = safecopy_to(endpoint, grant, copy_offset,
                              &client->buffer[client->tail],
                              copy_count * event_size)) != 0)
        return -retval;

    copy_offset += copy_count * event_size;
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

        if ((retval = safecopy_to(endpoint, grant, copy_offset, client->buffer,
                                  copy_count * event_size)) != 0)
            return -retval;
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
                          mgrant_id_t grant, unsigned int count, cdev_id_t id)
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
        client->req_grant = grant;
        client->req_size = count;
        client->req_id = id;

        return SUSPEND;
    }

    return evdev_copy_events(client, endpoint, grant, count);
}

static int handle_eviocgbit(struct input_dev* dev, unsigned int type,
                            size_t size, endpoint_t endpoint, mgrant_id_t grant)
{
    bitchunk_t* bits;
    size_t len;

    switch (type) {
    case 0:
        bits = dev->evbit;
        len = EV_MAX;
        break;
    case EV_KEY:
        bits = dev->keybit;
        len = KEY_MAX;
        break;
    case EV_REL:
        bits = dev->relbit;
        len = REL_MAX;
        break;
    case EV_ABS:
        bits = dev->absbit;
        len = ABS_MAX;
        break;
    case EV_MSC:
        bits = dev->mscbit;
        len = MSC_MAX;
        break;
    case EV_LED:
        bits = dev->ledbit;
        len = LED_MAX;
        break;
    case EV_SND:
        bits = dev->sndbit;
        len = SND_MAX;
        break;
    case EV_FF:
        bits = dev->ffbit;
        len = FF_MAX;
        break;
    case EV_SW:
        bits = dev->swbit;
        len = SW_MAX;
        break;
    default:
        return EINVAL;
    }

    len = BITCHUNKS(len) * sizeof(bitchunk_t);
    if (len > size) {
        len = size;
    }

    return safecopy_to(endpoint, grant, 0, bits, len);
}

static int str_to_user(const char* str, size_t maxlen, endpoint_t endpoint,
                       mgrant_id_t grant)
{
    int len;

    if (!str) return -ENOENT;

    len = strlen(str) + 1;
    if (len > maxlen) len = maxlen;

    return safecopy_to(endpoint, grant, 0, (void*)str, len);
}

long evdev_ioctl(struct input_handle* handle, int request, endpoint_t endpoint,
                 mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id)
{
    struct input_dev* dev = handle->dev;
    size_t size, len;
    char empty_str = '\0';
    int version = EV_VERSION;

    switch (request) {
    case EVIOCGVERSION:
        return safecopy_to(endpoint, grant, 0, &version, sizeof(version));
    case EVIOCGID:
        return safecopy_to(endpoint, grant, 0, &dev->input_id,
                           sizeof(struct input_id));
    }

    size = _IOC_SIZE(request);

#define EVIOC_MASK_SIZE(nr) ((nr) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))
    switch (EVIOC_MASK_SIZE(request)) {
    case EVIOCGKEY(0):
        len = BITCHUNKS(KEY_MAX) * sizeof(bitchunk_t);
        if (size < len) {
            len = size;
        }

        return safecopy_to(endpoint, grant, 0, &dev->key, len);

    case EVIOCGLED(0):
        len = BITCHUNKS(LED_MAX) * sizeof(bitchunk_t);
        if (size < len) {
            len = size;
        }

        return safecopy_to(endpoint, grant, 0, &dev->led, len);

    case EVIOCGSND(0):
        len = BITCHUNKS(SND_MAX) * sizeof(bitchunk_t);
        if (size < len) {
            len = size;
        }

        return safecopy_to(endpoint, grant, 0, &dev->snd, len);

    case EVIOCGSW(0):
        len = BITCHUNKS(SW_MAX) * sizeof(bitchunk_t);
        if (size < len) {
            len = size;
        }

        return safecopy_to(endpoint, grant, 0, &dev->sw, len);

    case EVIOCGNAME(0):
        return str_to_user(dev->name, size, endpoint, grant);
    case EVIOCGPHYS(0):
        return str_to_user(&empty_str, size, endpoint, grant);
    case EVIOCGUNIQ(0):
        return str_to_user(&empty_str, size, endpoint, grant);
    }

    if (_IOC_DIR(request) == _IOC_READ) {
        if ((_IOC_NR(request) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0)))
            return handle_eviocgbit(dev, _IOC_NR(request) & EV_MAX, size,
                                    endpoint, grant);
    }

    return EINVAL;
}

static struct input_handle_ops evdev_handle_ops = {
    .open = evdev_open,
    .close = evdev_close,
    .read = evdev_read,
    .ioctl = evdev_ioctl,
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
            retval = evdev_copy_events(client, client->caller,
                                       client->req_grant, client->req_size);
            chardriver_reply_io(client->caller, client->req_id, retval);

            client->suspended = FALSE;
            client->caller = NO_TASK;
            client->req_grant = GRANT_INVALID;
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
