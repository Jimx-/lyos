/*  
    (c)Copyright 2011 Jimx
    
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
    
#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "proto.h"
#include "lyos/proc.h"
#include "lyos/driver.h"
#include <lyos/sysutils.h>
#include <libsysfs/libsysfs.h>

PRIVATE void devman_init();

PUBLIC int main(int argc, char * argv[])
{
	devman_init();
    
    MESSAGE msg;

    while (1) {
        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case DM_DEVICE_ADD:
            msg.RETVAL = do_device_add(&msg);
            break;
        case DM_GET_DRIVER:
            msg.RETVAL = do_get_driver(&msg);
            break;
        case DM_BUS_REGISTER:
            msg.RETVAL = do_bus_register(&msg);
            break;
        case DM_DEVICE_REGISTER:
            msg.RETVAL = do_device_register(&msg);
            break;
        case DM_BUS_ATTR_ADD:
            msg.RETVAL = do_bus_attr_add(&msg);
            break;
        case DM_DEVICE_ATTR_ADD:
            msg.RETVAL = do_device_attr_add(&msg);
            break;
        case SYSFS_DYN_SHOW:
        case SYSFS_DYN_STORE:
            msg.CNT = sysfs_handle_dyn_attr(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}

PRIVATE void init_sysfs()
{
    sysfs_publish_domain("bus", SF_PRIV_OVERWRITE);
    sysfs_publish_domain("devices", SF_PRIV_OVERWRITE);
}

PRIVATE void devman_init()
{
	printl("DEVMAN: Device manager is running.\n");

    init_dd_map();
    init_bus();
    init_device();

    init_sysfs();

    /* Map init ramdisk */
    map_driver(MAKE_DEV(DEV_RD, MINOR_INITRD), DT_BLOCKDEV, TASK_RD);
}
