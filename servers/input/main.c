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
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/service.h>
#include <lyos/input.h>

#include <libsysfs/libsysfs.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "input.h"

#define INVALID_INPUT_ID (-1)

static int init_input();
static struct input_dev* input_allocate_dev(endpoint_t owner);
static struct input_dev* input_get_device(input_dev_id_t input_id);
static int input_attach_handler(struct input_dev* dev,
                                struct input_handler* handler);
static struct input_handle* input_get_handle(dev_t minor);
static void input_event(MESSAGE* msg);

static int input_open(dev_t minor, int access);
static int input_close(dev_t minor);
static ssize_t input_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                          unsigned int count, cdev_id_t id);
static ssize_t input_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                           unsigned int count, cdev_id_t id);
static int input_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                       cdev_id_t id);
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
    .cdr_other = input_other,
};

static int input_open(dev_t minor, int access)
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

static ssize_t input_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                          unsigned int count, cdev_id_t id)
{
    struct input_handle* handle;

    handle = input_get_handle(minor);
    if (!handle) {
        return ENXIO;
    }

    return handle->ops->read(handle, endpoint, buf, count, id);
}

static ssize_t input_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                           unsigned int count, cdev_id_t id)
{
    return -ENOSYS;
}

static int input_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                       cdev_id_t id)
{
    return ENOSYS;
}

static struct input_dev* input_allocate_dev(endpoint_t owner)
{
    static input_dev_id_t input_id = 0;
    struct input_dev* dev;

    dev = malloc(sizeof(*dev));
    if (dev) {
        dev->id = input_id++;
        dev->owner = owner;

        INIT_LIST_HEAD(&dev->h_list);
    }

    return dev;
}

static int input_register_device(MESSAGE* msg)
{
    struct input_dev* dev;
    struct input_handler* handler;
    MESSAGE conf_msg;
    struct device_info devinf;
    int retval = 0;

    dev = input_allocate_dev(msg->source);
    if (!dev) {
        retval = ENOMEM;
        goto reply;
    }

    dev->dev_id = msg->u.m_inputdriver_register_device.device_id;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "input%d", dev->id);
    devinf.bus = NO_BUS_ID;
    devinf.class = input_class;
    devinf.parent = dev->dev_id;
    devinf.devt = NO_DEV;

    retval = dm_device_register(&devinf, &dev->input_dev_id);
    if (retval) {
        free(dev);
        goto reply;
    }

    list_add(&dev->node, &input_dev_list);

    list_for_each_entry(handler, &input_handler_list, node)
    {
        input_attach_handler(dev, handler);
    }

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

static void input_other(MESSAGE* msg)
{
    int src = msg->source;

    switch (msg->type) {
    case INPUT_REGISTER_DEVICE:
        msg->RETVAL = input_register_device(msg);
        break;
    case INPUT_SEND_EVENT:
        input_event(msg);
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
