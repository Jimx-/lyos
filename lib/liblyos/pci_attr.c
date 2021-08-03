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
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/pci_utils.h>
#include <lyos/service.h>
#include "libsysfs/libsysfs.h"

u8 pci_attr_r8(int devind, u16 port)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_R8;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    pci_sendrec(BOTH, &msg);

    return (u8)msg.u.m3.m3i2;
}

u16 pci_attr_r16(int devind, u16 port)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_R16;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    pci_sendrec(BOTH, &msg);

    return (u8)msg.u.m3.m3i2;
}

u32 pci_attr_r32(int devind, u16 port)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_R32;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    pci_sendrec(BOTH, &msg);

    return msg.u.m3.m3i2;
}

int pci_attr_w8(int devind, u16 port, u8 value)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_W8;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;
    msg.u.m3.m3i4 = value;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}

int pci_attr_w16(int devind, u16 port, u16 value)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_W16;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;
    msg.u.m3.m3i4 = value;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}

int pci_attr_w32(int devind, u16 port, u32 value)
{
    MESSAGE msg;

    msg.type = PCI_ATTR_W8;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;
    msg.u.m3.m3i4 = value;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}

int pci_get_bar(int devind, u16 port, unsigned long* base, size_t* size,
                int* ioflag)
{
    MESSAGE msg;

    msg.type = PCI_GET_BAR;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = port;

    pci_sendrec(BOTH, &msg);

    if (msg.RETVAL == 0) {
        *base = (unsigned long)msg.u.m3.m3l1;
        *size = (size_t)msg.u.m3.m3l2;
        *ioflag = msg.u.m3.m3i4;
    }

    return msg.RETVAL;
}

int pci_find_capability(int devind, int cap)
{
    MESSAGE msg;

    msg.type = PCI_FIND_CAPABILITY;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = cap;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}

int pci_find_next_capability(int devind, int pos, int cap)
{
    MESSAGE msg;

    msg.type = PCI_FIND_NEXT_CAPABILITY;
    msg.u.m3.m3i2 = devind;
    msg.u.m3.m3i3 = cap;
    msg.u.m3.m3i4 = pos;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}
