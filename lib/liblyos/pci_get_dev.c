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

extern endpoint_t __pci_endpoint;

PUBLIC int pci_first_dev(int * devind, u16 * vid, u16 * did)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;
    
    msg.type = PCI_FIRST_DEV;

    send_recv(BOTH, __pci_endpoint, &msg);

    *devind = msg.u.m3.m3i2;
    *vid = msg.u.m3.m3i3;
    *did = msg.u.m3.m3i4;

    return msg.RETVAL;
}

PUBLIC int pci_next_dev(int * devind, u16 * vid, u16 * did)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;
    
    msg.type = PCI_NEXT_DEV;
    msg.u.m3.m3i2 = *devind;
    
    send_recv(BOTH, __pci_endpoint, &msg);

    *devind = msg.u.m3.m3i2;
    *vid = msg.u.m3.m3i3;
    *did = msg.u.m3.m3i4;

    return msg.RETVAL;
}
