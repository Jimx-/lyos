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
#include <lyos/driver.h>
#include <lyos/fs.h>
#include <lyos/proto.h>
#include <lyos/ipc.h>
#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"

#include <libdevman/libdevman.h>

PRIVATE endpoint_t cdev_driver_table[NR_DEVICES];

PRIVATE void init_cdev()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) cdev_driver_table[i] = NO_TASK;
}

PRIVATE int cdev_update(dev_t dev)
{

	dev_t major = MAJOR(dev);
   	cdev_driver_table[major] = dm_get_cdev_driver(dev);
    if (cdev_driver_table[major] < 0) cdev_driver_table[major] = NO_TASK;

    return cdev_driver_table[major] != NO_TASK;
}

PUBLIC struct fproc* cdev_get(dev_t dev)
{
	static int first = 1;

	if (first)	{
		init_cdev();
		first = 0;
	}

	dev_t major = MAJOR(dev);
	if (cdev_driver_table[major] == NO_TASK) 
		if (!cdev_update(dev)) return NULL;

	return vfs_endpt_proc(cdev_driver_table[major]);	
}

PRIVATE int cdev_sendrec(dev_t dev, MESSAGE* msg)
{
	struct fproc* driver = cdev_get(dev);
	if (!driver) return ENXIO;

	return send_recv(BOTH, driver->endpoint, msg);
}

PRIVATE int cdev_opcl(int op, dev_t dev)
{
	if (op != CDEV_OPEN && op != CDEV_CLOSE) {
		return EINVAL;
	}

	MESSAGE driver_msg;
	driver_msg.type = op;
	driver_msg.DEVICE = MINOR(dev);

	return cdev_sendrec(dev, &driver_msg);
}

PUBLIC int cdev_open(dev_t dev)
{
	return cdev_opcl(CDEV_OPEN, dev);
}

PUBLIC int cdev_close(dev_t dev)
{
	return cdev_opcl(CDEV_CLOSE, dev);
}

PUBLIC int cdev_io(int op, dev_t dev, endpoint_t src, vir_bytes buf, off_t pos,
	size_t count)
{
	if (op != CDEV_READ && op != CDEV_WRITE && op != CDEV_IOCTL) {
		return EINVAL;
	}

	MESSAGE driver_msg;
	driver_msg.type = op;
	driver_msg.DEVICE = MINOR(dev);
	driver_msg.BUF = buf;
	driver_msg.PROC_NR = src;
	if (op == CDEV_IOCTL) {
		driver_msg.REQUEST = count;
	} else {
		driver_msg.CNT = count;
	}

	int retval = cdev_sendrec(dev, &driver_msg);
	if (retval) return retval;

	if (driver_msg.type == SUSPEND_PROC) {
		return SUSPEND;
	}

	if (op == CDEV_IOCTL) return driver_msg.RETVAL;
	return driver_msg.CNT;
}
