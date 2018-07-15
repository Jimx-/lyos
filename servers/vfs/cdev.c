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
#include <lyos/driver.h>
#include <lyos/fs.h>
#include "const.h"
#include "types.h"
#include "global.h"
#include "proto.h"

#include <libdevman/libdevman.h>

PUBLIC struct cdmap cdmap[NR_DEVICES];

PRIVATE void init_cdev()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        cdmap[i].driver = NO_TASK;
		cdmap[i].select_busy = 0;
		cdmap[i].select_filp = NULL;
	}
}

PRIVATE int cdev_update(dev_t dev)
{
    dev_t major = MAJOR(dev);
    cdmap[major].driver = dm_get_cdev_driver(dev);
    if (cdmap[major].driver < 0) cdmap[major].driver = NO_TASK;

    return cdmap[major].driver != NO_TASK;
}

PUBLIC struct fproc* cdev_get(dev_t dev)
{
    static int first = 1;

    if (first)    {
        init_cdev();
        first = 0;
    }

    dev_t major = MAJOR(dev);
    if (cdmap[major].driver == NO_TASK) 
        if (!cdev_update(dev)) return NULL;

    return vfs_endpt_proc(cdmap[major].driver);    
}

PUBLIC struct cdmap* cdev_lookup_by_endpoint(endpoint_t driver_ep)
{
	int i;
	for (i = 0; i < NR_DEVICES; i++) {
		if (cdmap[i].driver != NO_TASK && cdmap[i].driver == driver_ep) {
			return &cdmap[i];
		}
	}

	return NULL;
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

PUBLIC int cdev_io(int op, dev_t dev, endpoint_t src, void* buf, off_t pos,
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
        driver_msg.POSITION = pos;
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

PUBLIC int cdev_mmap(dev_t dev, endpoint_t src, vir_bytes vaddr, off_t offset,
    size_t length, char** retaddr)
{
    MESSAGE driver_msg;
    driver_msg.type = CDEV_MMAP;
    driver_msg.DEVICE = MINOR(dev);
    driver_msg.ADDR = (void*) vaddr;
    driver_msg.PROC_NR = src;
    driver_msg.POSITION = offset;
    driver_msg.CNT = length;

    int retval = cdev_sendrec(dev, &driver_msg);
    if (retval) return retval;

    if (driver_msg.type == SUSPEND_PROC) {
        return SUSPEND;
    }

    *retaddr = driver_msg.ADDR;
    return driver_msg.RETVAL;
}

PUBLIC int cdev_select(dev_t dev, int ops)
{
    struct fproc* driver = cdev_get(dev);
    if (!driver) return ENXIO;

	MESSAGE driver_msg;
	memset(&driver_msg, 0, sizeof(MESSAGE));
	driver_msg.type = CDEV_SELECT;
	driver_msg.u.m_vfs_cdev_select.device = MINOR(dev);
	driver_msg.u.m_vfs_cdev_select.ops = ops;

	int retval = send_recv(SEND, driver->endpoint, &driver_msg);
	return retval;
}

PUBLIC int cdev_reply(MESSAGE* msg)
{
	switch (msg->type) {
	case CDEV_SELECT_REPLY1:
		do_select_cdev_reply1(msg->source, msg->DEVICE, msg->RETVAL);
		break;
	case CDEV_SELECT_REPLY2:
		do_select_cdev_reply2(msg->source, msg->DEVICE, msg->RETVAL);
		break;
	}
}
