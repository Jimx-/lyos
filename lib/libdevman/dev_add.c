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

#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <lyos/const.h>

#include <libasyncdriver/libasyncdriver.h>

#include "libdevman.h"

int dm_bdev_add(dev_t dev)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_ADD;
    msg.DEVICE = dev;
    msg.FLAGS = DT_BLOCKDEV;

    send_recv(BOTH, TASK_DEVMAN, &msg);

    return msg.RETVAL;
}

int dm_cdev_add(dev_t dev)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_ADD;
    msg.DEVICE = dev;
    msg.FLAGS = DT_CHARDEV;

    send_recv(BOTH, TASK_DEVMAN, &msg);

    return msg.RETVAL;
}

int dm_async_cdev_add(dev_t dev)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_ADD;
    msg.DEVICE = dev;
    msg.FLAGS = DT_CHARDEV;
    msg.u.m3.m3l1 = asyncdrv_worker_id();

    asyncdrv_sendrec(TASK_DEVMAN, &msg);

    return msg.RETVAL;
}
