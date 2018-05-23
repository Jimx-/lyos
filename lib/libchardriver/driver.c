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
#include <lyos/driver.h>
#include <lyos/proto.h>

#include "libchardriver/libchardriver.h"

PRIVATE int do_open(struct chardriver* cd, MESSAGE* msg)
{
    return cd->cdr_open(msg->DEVICE, 0);
}

PRIVATE int do_close(struct chardriver* cd, MESSAGE* msg)
{
    return cd->cdr_close(msg->DEVICE);
}

PRIVATE ssize_t do_rdwt(struct chardriver* cd, MESSAGE* msg)
{
    int do_write = 0;
    if (msg->type == CDEV_WRITE) do_write = 1;
    int minor = msg->DEVICE;
    u64 pos = msg->POSITION;
    endpoint_t ep = msg->PROC_NR;
    char* buf = msg->BUF;
    unsigned int count = msg->CNT;

    if (do_write && cd->cdr_write) return cd->cdr_write(minor, pos,
        ep, buf, count);
    else if (!do_write && cd->cdr_read) return cd->cdr_read(minor, pos,
        ep, buf, count);
    return -EIO;
}

PRIVATE int do_ioctl(struct chardriver* cd, MESSAGE* msg)
{
    int minor = msg->DEVICE;
    int request = msg->REQUEST;
    endpoint_t ep = msg->PROC_NR;
    char* buf = msg->BUF;

    return cd->cdr_ioctl(minor, request, ep, buf);
}

PRIVATE int do_mmap(struct chardriver* cd, MESSAGE* msg)
{
    if (!cd->cdr_mmap) return ENODEV;

    int minor = msg->DEVICE;
    endpoint_t ep = msg->PROC_NR;
    char* addr = msg->ADDR;
    off_t offset = msg->POSITION;
    size_t length = msg->CNT;

    char* retaddr;
    int retval = cd->cdr_mmap(minor, ep, addr, offset, length, &retaddr);

    msg->ADDR = retaddr;
    return retval;
}

PUBLIC void chardriver_process(struct chardriver* cd, MESSAGE* msg)
{
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (msg->source) {
        case INTERRUPT:
            if (cd->cdr_intr) cd->cdr_intr(msg->INTERRUPTS);
            break;
        case CLOCK:
            if (cd->cdr_alarm) cd->cdr_alarm(msg->TIMESTAMP);
            break;
        }
        return;
    }

    switch (msg->type) {
    case CDEV_OPEN:
        msg->RETVAL = do_open(cd, msg);
        break;

    case CDEV_CLOSE:
        msg->RETVAL = do_close(cd, msg);
        break;

    case CDEV_READ:
    case CDEV_WRITE: {
        int r = do_rdwt(cd, msg);
        if (r < 0) msg->RETVAL = -r;
        else {
            msg->RETVAL = 0;
            msg->CNT = r;
        }
        break;
    }
    case CDEV_IOCTL:
        msg->RETVAL = do_ioctl(cd, msg);
        break;

    case CDEV_MMAP:
        msg->RETVAL = do_mmap(cd, msg);
        break;

    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    send_recv(SEND, src, msg);
}

PUBLIC int chardriver_task(struct chardriver* cd)
{
    MESSAGE msg;
    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);
        
        chardriver_process(cd, &msg);
    }

    return 0;
}
