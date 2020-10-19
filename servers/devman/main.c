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

#include <lyos/types.h>
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
#include <lyos/netlink.h>
#include <sys/stat.h>
#include <libsysfs/libsysfs.h>
#include <libmemfs/libmemfs.h>

static void devman_init();
static void devfs_message_hook(MESSAGE* msg);

struct memfs_hooks fs_hooks = {
    .message_hook = devfs_message_hook,
};

int main(int argc, char* argv[])
{
    devman_init();

    struct memfs_stat root_stat;
    root_stat.st_mode =
        (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    root_stat.st_uid = SU_UID;
    root_stat.st_gid = 0;

    return memfs_start(NULL, &fs_hooks, &root_stat);
}

static void init_sysfs()
{
    sysfs_publish_domain("bus", SF_PRIV_OVERWRITE);
    sysfs_publish_domain("class", SF_PRIV_OVERWRITE);
    sysfs_publish_domain("devices", SF_PRIV_OVERWRITE);

    sysfs_publish_domain("dev", SF_PRIV_OVERWRITE);
    sysfs_publish_domain("dev.block", SF_PRIV_OVERWRITE);
    sysfs_publish_domain("dev.char", SF_PRIV_OVERWRITE);
}

static void devman_init()
{
    printl("DEVMAN: Device manager is running.\n");

    init_dd_map();
    init_bus();
    init_class();
    init_device();

    init_sysfs();
    uevent_init();

    /* Map init ramdisk */
    map_driver(MAKE_DEV(DEV_RD, MINOR_INITRD), DT_BLOCKDEV, TASK_RD);
}

static void devfs_message_hook(MESSAGE* msg)
{
    int reply = 1;

    switch (msg->type) {
    case DM_DEVICE_ADD:
        msg->RETVAL = do_device_add(msg);
        break;
    case DM_GET_DRIVER:
        msg->RETVAL = do_get_driver(msg);
        break;
    case DM_BUS_REGISTER:
        msg->u.m_devman_register_reply.status = do_bus_register(msg);
        break;
    case DM_CLASS_REGISTER:
        msg->u.m_devman_register_reply.status = do_class_register(msg);
        break;
    case DM_DEVICE_REGISTER:
        msg->u.m_devman_register_reply.status = do_device_register(msg);
        break;
    case DM_BUS_ATTR_ADD:
        msg->RETVAL = do_bus_attr_add(msg);
        break;
    case DM_DEVICE_ATTR_ADD:
        msg->RETVAL = do_device_attr_add(msg);
        break;
    case SYSFS_DYN_SHOW:
    case SYSFS_DYN_STORE:
        msg->CNT = sysfs_handle_dyn_attr(msg);
        break;
    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    if (reply) {
        msg->type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, msg->source, msg);
    }
}
