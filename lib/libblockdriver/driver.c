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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>

#include "libblockdriver/libblockdriver.h"

static int do_open(struct blockdriver* bd, MESSAGE* msg)
{
    return bd->bdr_open(msg->u.m_bdev_blockdriver_msg.minor, 0);
}

static int do_close(struct blockdriver* bd, MESSAGE* msg)
{
    return bd->bdr_close(msg->u.m_bdev_blockdriver_msg.minor);
}

static ssize_t do_rdwt(struct blockdriver* bd, MESSAGE* msg)
{
    int do_write = (msg->type == BDEV_WRITE) ? 1 : 0;
    int minor = msg->u.m_bdev_blockdriver_msg.minor;
    loff_t pos = msg->u.m_bdev_blockdriver_msg.pos;
    endpoint_t ep = msg->u.m_bdev_blockdriver_msg.endpoint;
    char* buf = msg->u.m_bdev_blockdriver_msg.buf;
    size_t count = msg->u.m_bdev_blockdriver_msg.count;

    return bd->bdr_readwrite(minor, do_write, pos, ep, buf, count);
}

static int do_ioctl(struct blockdriver* bd, MESSAGE* msg)
{
    int minor = msg->u.m_bdev_blockdriver_msg.minor;
    int request = msg->u.m_bdev_blockdriver_msg.request;
    endpoint_t ep = msg->u.m_bdev_blockdriver_msg.endpoint;
    char* buf = msg->u.m_bdev_blockdriver_msg.buf;

    return bd->bdr_ioctl(minor, request, ep, buf);
}

static void blockdriver_reply(MESSAGE* msg, ssize_t reply)
{
    MESSAGE reply_msg;

    memset(&reply_msg, 0, sizeof(reply_msg));
    reply_msg.type = BDEV_REPLY;
    reply_msg.u.m_blockdriver_bdev_reply.status = reply;

    send_recv(SEND, msg->source, &reply_msg);
}

void blockdriver_process(struct blockdriver* bd, MESSAGE* msg)
{
    ssize_t retval;
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (src) {
        case INTERRUPT:
            if (bd->bdr_intr) bd->bdr_intr(msg->INTERRUPTS);
            break;
        case CLOCK:
            if (bd->bdr_alarm) bd->bdr_alarm(msg->TIMESTAMP);
            break;
        }
        return;
    }

    switch (msg->type) {
    case BDEV_OPEN:
        retval = do_open(bd, msg);
        break;

    case BDEV_CLOSE:
        retval = do_close(bd, msg);
        break;

    case BDEV_READ:
    case BDEV_WRITE:
        retval = do_rdwt(bd, msg);
        break;

    case BDEV_IOCTL:
        retval = do_ioctl(bd, msg);
        break;

    default:
        retval = ENOSYS;
        break;
    }

    blockdriver_reply(msg, retval);
}

void blockdriver_task(struct blockdriver* bd)
{
    MESSAGE msg;
    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);

        blockdriver_process(bd, &msg);
    }
}
