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

PRIVATE int do_open(struct blockdriver* bd, MESSAGE* msg)
{
    return bd->bdr_open(msg->DEVICE, 0);
}

PRIVATE int do_close(struct blockdriver* bd, MESSAGE* msg)
{
    return bd->bdr_close(msg->DEVICE);
}

PRIVATE int do_rdwt(struct blockdriver* bd, MESSAGE* msg)
{
    int do_write = 0;
    if (msg->type == BDEV_WRITE) do_write = 1;
    int minor = msg->DEVICE;
    u64 pos = msg->POSITION;
    endpoint_t ep = msg->PROC_NR;
    char* buf = msg->BUF;
    unsigned int count = msg->CNT;

    return bd->bdr_readwrite(minor, do_write, pos,
        ep, buf, count);
}

PRIVATE int do_ioctl(struct blockdriver* bd, MESSAGE* msg)
{
    int minor = msg->DEVICE;
    int request = msg->REQUEST;
    endpoint_t ep = msg->PROC_NR;
    char* buf = msg->BUF;

    return bd->bdr_ioctl(minor, request, ep, buf);
}

PUBLIC void blockdriver_process(struct blockdriver* bd, MESSAGE* msg)
{
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
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
        msg->RETVAL = do_open(bd, msg);
        break;

    case BDEV_CLOSE:
        msg->RETVAL = do_close(bd, msg);
        break;

    case BDEV_READ:
    case BDEV_WRITE:
        msg->RETVAL = do_rdwt(bd, msg);
        break;

    case BDEV_IOCTL:
        msg->RETVAL = do_ioctl(bd, msg);
        break;

    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    send_recv(SEND, src, msg);
}

PUBLIC void blockdriver_task(struct blockdriver* bd)
{
    MESSAGE msg;
    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);
        
        blockdriver_process(bd, &msg);
    }
}

PUBLIC int announce_blockdev(char * name, dev_t dev)
{
    MESSAGE msg;

    msg.type = ANNOUNCE_DEVICE;
    msg.BUF = name;
    msg.NAME_LEN = strlen(name);
    msg.PROC_NR = get_endpoint();
    msg.DEVICE = dev;
    msg.FLAGS = DT_BLOCKDEV;

    send_recv(BOTH, TASK_DEVMAN, &msg);

    return msg.RETVAL;
}
