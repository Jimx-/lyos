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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/driver.h"
#include "lyos/proto.h"

PUBLIC void dev_driver_task(struct dev_driver * dd)
{
	MESSAGE msg;
	while (1) {
		send_recv(RECEIVE, ANY, &msg);
		int src = msg.source;

		switch (msg.type) {
		case DEV_OPEN:
			msg.RETVAL = (*dd->dev_open)(&msg);
			break;

		case DEV_CLOSE:
			msg.RETVAL = (*dd->dev_close)(&msg);
			break;

		case DEV_READ:
			msg.RETVAL = (*dd->dev_read)(&msg);
			break;

		case DEV_WRITE:
			msg.RETVAL = (*dd->dev_write)(&msg);
			break;

		case DEV_IOCTL:
			msg.RETVAL = (*dd->dev_ioctl)(&msg);
			break;

		default:
			msg.RETVAL = ENOSYS;
			break;
		}

		send_recv(SEND, src, &msg);
	}
}

PUBLIC int announce_blockdev(char * name, dev_t dev)
{
	MESSAGE msg;

	msg.type = ANNOUNCE_DEVICE;
	msg.BUF = name;
	msg.NAME_LEN = strlen(name);
	msg.PROC_NR = getpid();
	msg.DEVICE = dev;
	msg.FLAGS = DT_BLOCKDEV;

	send_recv(BOTH, TASK_DEVMAN, &msg);

	return msg.RETVAL;
}

PUBLIC int announce_chardev(char * name, dev_t dev)
{
	MESSAGE msg;

	msg.type = ANNOUNCE_DEVICE;
	msg.BUF = name;
	msg.NAME_LEN = strlen(name);
	msg.PROC_NR = getpid();
	msg.DEVICE = dev;
	msg.FLAGS = DT_CHARDEV;

	send_recv(BOTH, TASK_DEVMAN, &msg);

	return msg.RETVAL;
}

PUBLIC endpoint_t get_blockdev_driver(dev_t dev)
{
    MESSAGE msg;

    msg.type = GET_DRIVER;
    msg.DEVICE = dev;
    msg.FLAGS = DT_BLOCKDEV;

    send_recv(BOTH, TASK_DEVMAN, &msg);

    return (msg.RETVAL != 0) ? -msg.RETVAL : msg.PID;
}

PUBLIC endpoint_t get_chardev_driver(dev_t dev)
{
    MESSAGE msg;

    msg.type = GET_DRIVER;
    msg.DEVICE = dev;
    msg.FLAGS = DT_CHARDEV;

    send_recv(BOTH, TASK_DEVMAN, &msg);

    return (msg.RETVAL != 0) ? -msg.RETVAL : msg.PID;
}

/*****************************************************************************
 *                                rw_sector
 *****************************************************************************/
/**
 * <Ring 1> R/W a sector via messaging with the corresponding driver.
 * 
 * @param io_type  DEV_READ or DEV_WRITE
 * @param dev      device nr
 * @param pos      Byte offset from/to where to r/w.
 * @param bytes    r/w count in bytes.
 * @param proc_nr  To whom the buffer belongs.
 * @param buf      r/w buffer.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int rw_sector(int io_type, int dev, u64 pos, int bytes, int proc_nr,
		     void* buf)
{
	MESSAGE driver_msg;
    endpoint_t driver_nr = get_blockdev_driver(dev);
    if (driver_nr < 0) return -driver_nr;

	driver_msg.type		= io_type;
	driver_msg.DEVICE	= MINOR(dev);
	driver_msg.POSITION	= pos;
	driver_msg.BUF		= buf;
	driver_msg.CNT		= bytes;
	driver_msg.PROC_NR	= proc_nr;

	send_recv(BOTH, driver_nr, &driver_msg);

	return driver_msg.RETVAL;
}

