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
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/service.h>
#include "libsysfs/libsysfs.h"

extern endpoint_t __pci_endpoint;

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

#ifdef __i386__
    send_recv(BOTH, TASK_PCI, &msg);
#else
    send_recv(BOTH, __pci_endpoint, &msg);
#endif

    return (u8)msg.u.m3.m3i2;
}

PUBLIC u16 pci_attr_r16(int devind, u16 port)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;

    msg.type = PCI_ATTR_R16;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

#ifdef __i386__
    send_recv(BOTH, TASK_PCI, &msg);
#else
    send_recv(BOTH, __pci_endpoint, &msg);
#endif

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

#ifdef __i386__
    send_recv(BOTH, TASK_PCI, &msg);
#else
    send_recv(BOTH, __pci_endpoint, &msg);
#endif

    return msg.u.m3.m3i2;
}

PUBLIC int pci_attr_w16(int devind, u16 port, u16 value)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;

    msg.type = PCI_ATTR_W16;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;
    msg.u.m3.m3i4 = value;

#ifdef __i386__
    send_recv(BOTH, TASK_PCI, &msg);
#else
    send_recv(BOTH, __pci_endpoint, &msg);
#endif

    return msg.RETVAL;
}

int pci_get_bar(int devind, u16 port, u32* base, u32* size, int* ioflag)
{
    u32 v;
    int retval;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    MESSAGE msg;

    msg.type = PCI_GET_BAR;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

#ifdef __i386__
    send_recv(BOTH, TASK_PCI, &msg);
#else
    send_recv(BOTH, __pci_endpoint, &msg);
#endif

    if (msg.RETVAL == 0) {
        *base = msg.u.m3.m3i2;
        *size = msg.u.m3.m3i3;
        *ioflag = msg.u.m3.m3i4;
    }

    return msg.RETVAL;
}
