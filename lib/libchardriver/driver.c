/*  This file is part of Lyos.

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
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>

#include "libchardriver/libchardriver.h"

static int do_open(struct chardriver* cd, MESSAGE* msg)
{
    if (cd->cdr_open == NULL) return 0;
    return cd->cdr_open(msg->u.m_vfs_cdev_openclose.minor,
                        msg->u.m_vfs_cdev_openclose.access,
                        msg->u.m_vfs_cdev_openclose.user);
}

static int do_close(struct chardriver* cd, MESSAGE* msg)
{
    if (cd->cdr_close == NULL) return 0;
    return cd->cdr_close(msg->u.m_vfs_cdev_openclose.minor);
}

static ssize_t do_rdwt(struct chardriver* cd, MESSAGE* msg)
{
    int do_write = 0;
    if (msg->type == CDEV_WRITE) do_write = 1;

    int minor = msg->u.m_vfs_cdev_readwrite.minor;
    cdev_id_t id = msg->u.m_vfs_cdev_readwrite.id;
    u64 pos = msg->u.m_vfs_cdev_readwrite.pos;
    endpoint_t ep = msg->source;
    mgrant_id_t grant = msg->u.m_vfs_cdev_readwrite.grant;
    unsigned int count = msg->u.m_vfs_cdev_readwrite.count;
    int flags = msg->u.m_vfs_cdev_readwrite.flags;

    if (do_write && cd->cdr_write)
        return cd->cdr_write(minor, pos, ep, grant, count, flags, id);
    else if (!do_write && cd->cdr_read)
        return cd->cdr_read(minor, pos, ep, grant, count, flags, id);
    return -EIO;
}

static int do_ioctl(struct chardriver* cd, MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int minor = msg->u.m_vfs_cdev_readwrite.minor;
    cdev_id_t id = msg->u.m_vfs_cdev_readwrite.id;
    int request = msg->u.m_vfs_cdev_readwrite.request;
    endpoint_t user_ep = msg->u.m_vfs_cdev_readwrite.endpoint;
    mgrant_id_t grant = msg->u.m_vfs_cdev_readwrite.grant;
    int flags = msg->u.m_vfs_cdev_readwrite.flags;

    if (cd->cdr_ioctl == NULL) return ENOTTY;

    return cd->cdr_ioctl(minor, request, src, grant, flags, user_ep, id);
}

static int do_mmap(struct chardriver* cd, MESSAGE* msg)
{
    if (!cd->cdr_mmap) return ENODEV;

    int minor = msg->u.m_vfs_cdev_mmap.minor;
    endpoint_t ep = msg->u.m_vfs_cdev_mmap.endpoint;
    char* addr = msg->u.m_vfs_cdev_mmap.addr;
    off_t offset = msg->u.m_vfs_cdev_mmap.pos;
    size_t length = msg->u.m_vfs_cdev_mmap.count;

    char* retaddr;
    int retval = cd->cdr_mmap(minor, ep, addr, offset, length, &retaddr);

    msg->u.m_vfs_cdev_mmap.addr = retaddr;
    return retval;
}

static int do_select(struct chardriver* cd, MESSAGE* msg)
{
    int minor = msg->u.m_vfs_cdev_select.minor;
    int ops = msg->u.m_vfs_cdev_select.ops;
    endpoint_t endpoint = msg->source;

    if (cd->cdr_select == NULL) return EBADF;
    return cd->cdr_select(minor, ops, endpoint);
}

void chardriver_process(struct chardriver* cd, MESSAGE* msg)
{
    int retval = 0;

    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case INTERRUPT:
            if (cd->cdr_intr) cd->cdr_intr(msg->INTERRUPTS);
            break;
        case CLOCK:
            if (cd->cdr_alarm) cd->cdr_alarm(msg->TIMESTAMP);
            break;
        default:
            if (cd->cdr_other) cd->cdr_other(msg);
            break;
        }
        return;
    }

    switch (msg->type) {
    case CDEV_OPEN:
        retval = do_open(cd, msg);
        break;
    case CDEV_CLOSE:
        retval = do_close(cd, msg);
        break;
    case CDEV_READ:
    case CDEV_WRITE:
        retval = do_rdwt(cd, msg);
        break;
    case CDEV_IOCTL:
        retval = do_ioctl(cd, msg);
        break;
    case CDEV_MMAP:
        retval = do_mmap(cd, msg);
        break;
    case CDEV_SELECT:
        retval = do_select(cd, msg);
        break;
    default:
        if (cd->cdr_other) {
            cd->cdr_other(msg);
            return; /* no reply */
        } else {
            retval = ENOSYS;
        }
        break;
    }

    chardriver_reply(msg, retval);
}

int chardriver_task(struct chardriver* cd)
{
    MESSAGE msg;
    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &msg);

        chardriver_process(cd, &msg);
    }

    return 0;
}

void chardriver_reply(MESSAGE* msg, int retval)
{
    MESSAGE reply_msg;

    if (retval == SUSPEND) {
        switch (msg->type) {
        case CDEV_READ:
        case CDEV_WRITE:
        case CDEV_IOCTL:
            return;
        default:
            panic("chardriver: bad request to suspend: %d", msg->type);
        }
    }

    memset(&reply_msg, 0, sizeof(MESSAGE));
    endpoint_t dest_ep = TASK_FS;

    switch (msg->type) {
    case CDEV_OPEN:
    case CDEV_CLOSE:
        dest_ep = msg->source;
        reply_msg.type = CDEV_REPLY;
        reply_msg.u.m_vfs_cdev_reply.status = retval;
        reply_msg.u.m_vfs_cdev_reply.id = msg->u.m_vfs_cdev_openclose.id;
        break;
    case CDEV_READ:
    case CDEV_WRITE:
    case CDEV_IOCTL:
        reply_msg.type = CDEV_REPLY;
        reply_msg.u.m_vfs_cdev_reply.status = retval;
        reply_msg.u.m_vfs_cdev_reply.id = msg->u.m_vfs_cdev_readwrite.id;
        break;
    case CDEV_MMAP:
        reply_msg.type = CDEV_REPLY;
        reply_msg.u.m_vfs_cdev_reply.status = retval;
        reply_msg.u.m_vfs_cdev_reply.retaddr = msg->u.m_vfs_cdev_mmap.addr;
        reply_msg.u.m_vfs_cdev_reply.id = msg->u.m_vfs_cdev_mmap.id;
        break;
    case CDEV_SELECT:
        reply_msg.type = CDEV_REPLY;
        reply_msg.u.m_vfs_cdev_reply.status = retval;
        reply_msg.u.m_vfs_cdev_reply.id = msg->u.m_vfs_cdev_select.id;
        break;
    }

    send_recv(SEND, dest_ep, &reply_msg);
}

void chardriver_reply_io(endpoint_t endpoint, cdev_id_t id, int retval)
{
    MESSAGE msg;
    memset(&msg, 0, sizeof(MESSAGE));

    msg.type = CDEV_REPLY;
    msg.u.m_vfs_cdev_reply.status = retval;
    msg.u.m_vfs_cdev_reply.id = id;

    if (send_recv(SEND, endpoint, &msg) != 0) {
        panic("chardriver: failed to reply to vfs");
    }
}

void chardriver_poll_notify(dev_t minor, __poll_t status)
{
    MESSAGE msg;
    memset(&msg, 0, sizeof(MESSAGE));

    msg.type = CDEV_POLL_NOTIFY;
    msg.u.m_vfs_cdev_poll_notify.status = status;
    msg.u.m_vfs_cdev_poll_notify.minor = minor;

    if (send_recv(SEND, TASK_FS, &msg) != 0) {
        panic("chardriver: failed to reply to vfs");
    }
}

int chardriver_get_minor(MESSAGE* msg, dev_t* minor)
{
    switch (msg->type) {
    case CDEV_OPEN:
    case CDEV_CLOSE:
        *minor = msg->u.m_vfs_cdev_openclose.minor;
        return 0;
    case CDEV_READ:
    case CDEV_WRITE:
    case CDEV_IOCTL:
        *minor = msg->u.m_vfs_cdev_readwrite.minor;
        return 0;
    case CDEV_SELECT:
        *minor = msg->u.m_vfs_cdev_select.minor;
        return 0;
    }

    return EINVAL;
}
