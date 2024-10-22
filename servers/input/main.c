/*
    This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

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
#include <lyos/service.h>
#include <lyos/input.h>
#include <lyos/sysutils.h>
#include <asm/page.h>

#include <libsysfs/libsysfs.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "input.h"
#include "lyos/bitmap.h"

#define INVALID_INPUT_ID (-1)

static int init_input();
static struct input_dev* input_allocate_dev(endpoint_t owner);
static struct input_dev* input_get_device(input_dev_id_t input_id);
static int input_attach_handler(struct input_dev* dev,
                                struct input_handler* handler);
static struct input_handle* input_get_handle(dev_t minor);
static void input_event(MESSAGE* msg);

static int input_open(dev_t minor, int access, endpoint_t user_endpt);
static int input_close(dev_t minor);
static ssize_t input_read(dev_t minor, u64 pos, endpoint_t endpoint,
                          mgrant_id_t grant, unsigned int count, int flags,
                          cdev_id_t id);
static ssize_t input_write(dev_t minor, u64 pos, endpoint_t endpoint,
                           mgrant_id_t grant, unsigned int count, int flags,
                           cdev_id_t id);
static int input_ioctl(dev_t minor, int request, endpoint_t endpoint,
                       mgrant_id_t grant, int flags, endpoint_t user_endpoint,
                       cdev_id_t id);
static int input_select(dev_t minor, int ops, endpoint_t endpoint);
static void input_other(MESSAGE* msg);

static const char* name = "input";
class_id_t input_class;

static struct list_head input_dev_list;
static struct list_head input_handler_list;
static struct list_head input_handle_list;

static struct chardriver input_driver = {
    .cdr_open = input_open,
    .cdr_close = input_close,
    .cdr_read = input_read,
    .cdr_write = input_write,
    .cdr_ioctl = input_ioctl,
    .cdr_select = input_select,
    .cdr_other = input_other,
};

static int input_open(dev_t minor, int access, endpoint_t user_endpt)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) {
        return ENXIO;
    }

    return handle->ops->open(handle);
}

static int input_close(dev_t minor)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) {
        return ENXIO;
    }

    return handle->ops->close(handle);
}

static ssize_t input_read(dev_t minor, u64 pos, endpoint_t endpoint,
                          mgrant_id_t grant, unsigned int count, int flags,
                          cdev_id_t id)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) {
        return ENXIO;
    }

    return handle->ops->read(handle, endpoint, grant, count, flags, id);
}

static ssize_t input_write(dev_t minor, u64 pos, endpoint_t endpoint,
                           mgrant_id_t grant, unsigned int count, int flags,
                           cdev_id_t id)
{
    return -ENOSYS;
}

static int input_ioctl(dev_t minor, int request, endpoint_t endpoint,
                       mgrant_id_t grant, int flags, endpoint_t user_endpoint,
                       cdev_id_t id)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) return ENXIO;

    return handle->ops->ioctl(handle, request, endpoint, grant, user_endpoint,
                              id);
}

static int input_select(dev_t minor, int ops, endpoint_t endpoint)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) return ENXIO;

    return handle->ops->poll(handle, ops, endpoint);
}

static struct input_dev* input_allocate_dev(endpoint_t owner)
{
    static input_dev_id_t input_id = 0;
    struct input_dev* dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        memset(dev, 0, sizeof(*dev));
        dev->id = input_id++;
        dev->owner = owner;

        INIT_LIST_HEAD(&dev->h_list);
    }

    return dev;
}

static int input_copy_bits(struct input_dev* dev, endpoint_t endpoint,
                           struct input_dev_bits* bits)
{
    int retval;

#define COPY_BIT(name)                                            \
    do {                                                          \
        retval = data_copy(SELF, dev->name, endpoint, bits->name, \
                           sizeof(dev->name));                    \
        if (retval) return retval;                                \
    } while (0)

    COPY_BIT(evbit);
    COPY_BIT(keybit);
    COPY_BIT(relbit);
    COPY_BIT(absbit);
    COPY_BIT(mscbit);
    COPY_BIT(ledbit);
    COPY_BIT(sndbit);
    COPY_BIT(ffbit);
    COPY_BIT(swbit);

    return retval;
}

#define INPUT_DEV_STRING_ATTR_SHOW(name)                                \
    static ssize_t input_dev_show_##name(struct device_attribute* attr, \
                                         char* buf, size_t size)        \
    {                                                                   \
        struct input_dev* input_dev = (struct input_dev*)attr->cb_data; \
        return snprintf(buf, size, "%s\n", input_dev->name);            \
    }

INPUT_DEV_STRING_ATTR_SHOW(name)

static int input_bits_to_string(char* buf, int buf_size, bitchunk_t bits,
                                int skip_empty)
{
    return bits || !skip_empty ? snprintf(buf, buf_size, "%lx", bits) : 0;
}

static int input_print_bitmap(char* buf, int buf_size, bitchunk_t* bitmap,
                              int max, int add_cr)
{
    int i;
    int len = 0;
    int skip_empty = TRUE;

    for (i = BITCHUNKS(max) - 1; i >= 0; i--) {
        len += input_bits_to_string(buf + len, max(buf_size - len, 0),
                                    bitmap[i], skip_empty);
        if (len) {
            skip_empty = FALSE;
            if (i > 0) len += snprintf(buf + len, max(buf_size - len, 0), " ");
        }
    }

    if (len == 0) len = snprintf(buf, buf_size, "%d", 0);

    if (add_cr) len += snprintf(buf + len, max(buf_size - len, 0), "\n");

    return len;
}

#define INPUT_DEV_CAP_ATTR(ev, bm)                                             \
    static ssize_t input_dev_show_cap_##bm(struct device_attribute* attr,      \
                                           char* buf, size_t size)             \
    {                                                                          \
        struct input_dev* input_dev = (struct input_dev*)attr->cb_data;        \
        int len =                                                              \
            input_print_bitmap(buf, size, input_dev->bm##bit, ev##_MAX, TRUE); \
        return len > size ? size : len;                                        \
    }

INPUT_DEV_CAP_ATTR(EV, ev)
INPUT_DEV_CAP_ATTR(KEY, key)
INPUT_DEV_CAP_ATTR(REL, rel)
INPUT_DEV_CAP_ATTR(ABS, abs)

static int input_register_device(MESSAGE* msg)
{
    struct input_dev* dev;
    struct input_handler* handler;
    MESSAGE conf_msg;
    struct device_info devinf;
    struct input_dev_bits dev_bits;
    struct device_attribute attr;
    int retval = 0;

    dev = input_allocate_dev(msg->source);
    if (!dev) {
        retval = ENOMEM;
        goto reply;
    }

    dev->dev_id = msg->u.m_inputdriver_register_device.dev_id;

    retval = data_copy(SELF, &dev_bits, msg->source,
                       msg->u.m_inputdriver_register_device.dev_bits,
                       sizeof(dev_bits));
    if (retval) goto reply_free_dev;

    retval = input_copy_bits(dev, msg->source, &dev_bits);
    if (retval) goto reply_free_dev;

    retval = data_copy(SELF, &dev->input_id, msg->source,
                       msg->u.m_inputdriver_register_device.input_id,
                       sizeof(dev->input_id));
    if (retval) goto reply_free_dev;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(dev->name, sizeof(dev->name), "input%d", dev->id);
    snprintf(devinf.name, sizeof(devinf.name), "input%d", dev->id);
    devinf.bus = NO_BUS_ID;
    devinf.class = input_class;
    devinf.parent = dev->dev_id;
    devinf.devt = NO_DEV;

    retval = dm_device_register(&devinf, &dev->input_dev_id);
    if (retval) goto reply_free_dev;

    list_add(&dev->node, &input_dev_list);

    list_for_each_entry(handler, &input_handler_list, node)
    {
        input_attach_handler(dev, handler);
    }

    dm_init_device_attr(&attr, dev->input_dev_id, "name", SF_PRIV_OVERWRITE,
                        dev, input_dev_show_name, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, dev->input_dev_id, "capabilities/ev",
                        SF_PRIV_OVERWRITE, dev, input_dev_show_cap_ev, NULL);
    dm_device_attr_add(&attr);
    dm_init_device_attr(&attr, dev->input_dev_id, "capabilities/key",
                        SF_PRIV_OVERWRITE, dev, input_dev_show_cap_key, NULL);
    dm_device_attr_add(&attr);
    dm_init_device_attr(&attr, dev->input_dev_id, "capabilities/rel",
                        SF_PRIV_OVERWRITE, dev, input_dev_show_cap_rel, NULL);
    dm_device_attr_add(&attr);
    dm_init_device_attr(&attr, dev->input_dev_id, "capabilities/abs",
                        SF_PRIV_OVERWRITE, dev, input_dev_show_cap_abs, NULL);
    dm_device_attr_add(&attr);

reply_free_dev:
    if (retval) free(dev);

reply:
    conf_msg.u.m_input_conf.status = retval;

    if (!retval) {
        conf_msg.u.m_input_conf.id = dev->id;
    }

    send_recv(SEND_NONBLOCK, msg->source, &conf_msg);

    return SUSPEND;
}

int input_open_device(struct input_handle* handle)
{
    handle->open++;

    return 0;
}

void input_close_device(struct input_handle* handle) { handle->open--; }

static struct input_dev* input_get_device(input_dev_id_t input_id)
{
    struct input_dev* dev;

    list_for_each_entry(dev, &input_dev_list, node)
    {
        if (dev->id == input_id) {
            return dev;
        }
    }

    return NULL;
}

static int input_attach_handler(struct input_dev* dev,
                                struct input_handler* handler)
{
    return handler->connect(handler, dev);
}

int input_register_handler(struct input_handler* handler)
{
    struct input_dev* dev;

    INIT_LIST_HEAD(&handler->h_list);

    list_add(&handler->node, &input_handler_list);

    list_for_each_entry(dev, &input_dev_list, node)
    {
        input_attach_handler(dev, handler);
    }

    return 0;
}

static struct input_handle* input_get_handle(dev_t minor)
{
    struct input_handle* handle;

    list_for_each_entry(handle, &input_handle_list, node)
    {
        if (handle->minor == minor) {
            return handle;
        }
    }

    return NULL;
}

int input_register_handle(struct input_handle* handle)
{
    list_add(&handle->d_node, &handle->dev->h_list);
    list_add(&handle->h_node, &handle->handler->h_list);

    if (handle->minor != NO_DEV) {
        list_add(&handle->node, &input_handle_list);
    }

    return 0;
}

static int input_handle_event(struct input_dev* dev, unsigned int type,
                              unsigned int code, int value)
{
    struct input_handle* handle;
    int handled = FALSE;

    list_for_each_entry(handle, &dev->h_list, d_node)
    {
        if (handle->open) {
            handle->handler->event(handle, type, code, value);
            handled = TRUE;
        }
    }

    return handled;
}

static void input_event(MESSAGE* msg)
{
    struct input_dev* dev;
    MESSAGE msg2tty;
    input_dev_id_t input_id;
    unsigned int type, code;
    int value;
    int handled;

    input_id = msg->u.m_inputdriver_input_event.id;
    type = msg->u.m_inputdriver_input_event.type;
    code = msg->u.m_inputdriver_input_event.code;
    value = msg->u.m_inputdriver_input_event.value;

    dev = input_get_device(input_id);
    if (!dev) {
        return;
    }

    handled = input_handle_event(dev, type, code, value);

    if (!handled) {
        /* just tell TTY */
        msg2tty.type = INPUT_TTY_EVENT;
        msg2tty.u.m_input_tty_event.type = type;
        msg2tty.u.m_input_tty_event.code = code;
        msg2tty.u.m_input_tty_event.value = value;

        send_recv(SEND, TASK_TTY, &msg2tty);
    }
}

static void process_sysfs_events(void)
{
    char key[PATH_MAX];
    int type, event;
    endpoint_t endpoint;
    int retval;

    while (sysfs_get_event(key, &type, &event) == 0) {
        if ((retval = sysfs_retrieve_u32(key, &endpoint)) != 0) {
            continue;
        }
    }
}

static void input_other(MESSAGE* msg)
{
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (src) {
        case TASK_SYSFS:
            process_sysfs_events();
            break;
        }
        return;
    }

    switch (msg->type) {
    case INPUT_REGISTER_DEVICE:
        msg->RETVAL = input_register_device(msg);
        break;
    case INPUT_SEND_EVENT:
        input_event(msg);
        msg->RETVAL = SUSPEND;
        break;
    case DM_DEVICE_ATTR_SHOW:
    case DM_DEVICE_ATTR_STORE:
        dm_device_attr_handle(msg);
        msg->RETVAL = SUSPEND;
        break;
    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    if (msg->RETVAL != SUSPEND) {
        msg->type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, msg);
    }
}

static int init_input()
{
    MESSAGE msg;
    int retval;

    INIT_LIST_HEAD(&input_dev_list);
    INIT_LIST_HEAD(&input_handler_list);
    INIT_LIST_HEAD(&input_handle_list);

    retval = dm_class_register("input", &input_class);
    if (retval) {
        panic("%s: failed to register input class", name);
    }

    evdev_init();

    if ((retval = sysfs_subscribe("services\\.inputdrv\\.[^.]*\\.endpoint",
                                  SF_CHECK_NOW)) != 0)
        panic("%s: failed to subscribe to input driver events (%d)", name,
              retval);

    /* tell TTY that we're up */
    memset(&msg, 0, sizeof(msg));
    msg.type = INPUT_TTY_UP;

    send_recv(SEND, TASK_TTY, &msg);

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(init_input);
    serv_init();

    chardriver_task(&input_driver);

    return 0;
}
