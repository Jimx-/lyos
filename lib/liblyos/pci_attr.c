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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/config.h>
#include <lyos/type.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/service.h>
#include <lyos/ipc.h>
#include "libsysfs/libsysfs.h"

extern endpoint_t __pci_endpoint = TASK_PCI;

PUBLIC u8 pci_attr_r8(int devind, u16 port)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;
    
    msg.type = PCI_ATTR_R8;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    send_recv(BOTH, TASK_PCI, &msg);

    return (u8)msg.u.m3.m3i2;
}

PUBLIC u32 pci_attr_r32(int devind, u16 port)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;
    
    msg.type = PCI_ATTR_R32;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    send_recv(BOTH, TASK_PCI, &msg);

    return msg.u.m3.m3i2;
}
