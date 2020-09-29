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
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>

#include <libblockdriver/libblockdriver.h>

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
    endpoint_t src = msg->source;
    int do_write = msg->type == BDEV_WRITE;
    int minor = msg->u.m_bdev_blockdriver_msg.minor;
    loff_t pos = msg->u.m_bdev_blockdriver_msg.pos;
    mgrant_id_t grant = msg->u.m_bdev_blockdriver_msg.grant;
    size_t count = msg->u.m_bdev_blockdriver_msg.count;
    struct iovec_grant iov;

    iov.iov_grant = grant;
    iov.iov_len = count;

    return bd->bdr_readwrite(minor, do_write, pos, src, &iov, 1);
}

static ssize_t do_vrdwt(struct blockdriver* bd, MESSAGE* msg)
{
    struct iovec_grant iovec[NR_IOREQS];
    endpoint_t src = msg->source;
    int do_write = msg->type == BDEV_WRITEV;
    int minor = msg->u.m_bdev_blockdriver_msg.minor;
    loff_t pos = msg->u.m_bdev_blockdriver_msg.pos;
    size_t count = msg->u.m_bdev_blockdriver_msg.count;
    mgrant_id_t grant = msg->u.m_bdev_blockdriver_msg.grant;
    int retval;

    if (count > NR_IOREQS) {
        count = NR_IOREQS;
    }

    if ((retval = safecopy_from(src, grant, 0, iovec,
                                sizeof(iovec[0]) * count)) != 0)
        return -EINVAL;

    return bd->bdr_readwrite(minor, do_write, pos, src, iovec, count);
}

static int do_ioctl(struct blockdriver* bd, MESSAGE* msg)
{
    endpoint_t src = msg->source;
    int minor = msg->u.m_bdev_blockdriver_msg.minor;
    int request = msg->u.m_bdev_blockdriver_msg.request;
    mgrant_id_t grant = msg->u.m_bdev_blockdriver_msg.grant;
    endpoint_t user_endpoint = msg->u.m_bdev_blockdriver_msg.user_endpoint;

    return bd->bdr_ioctl(minor, request, src, grant, user_endpoint);
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

    case BDEV_READV:
    case BDEV_WRITEV:
        retval = do_vrdwt(bd, msg);
        break;

    case BDEV_IOCTL:
        retval = do_ioctl(bd, msg);
        break;

    default:
        if (bd->bdr_other) {
            bd->bdr_other(msg);
            return;
        }

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
