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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/pci_utils.h>
#include <libdevman/libdevman.h>

int pci_first_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id)
{
    MESSAGE msg;

    msg.type = PCI_FIRST_DEV;

    pci_sendrec(BOTH, &msg);

    *devind = msg.u.m3.m3i2;
    *vid = msg.u.m3.m3i3;
    *did = msg.u.m3.m3i4;

    if (dev_id) {
        *dev_id = (device_id_t)msg.u.m3.m3l1;
    }

    return msg.RETVAL;
}

int pci_next_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id)
{
    MESSAGE msg;

    msg.type = PCI_NEXT_DEV;
    msg.u.m3.m3i2 = *devind;

    pci_sendrec(BOTH, &msg);

    *devind = msg.u.m3.m3i2;
    *vid = msg.u.m3.m3i3;
    *did = msg.u.m3.m3i4;

    if (dev_id) {
        *dev_id = (device_id_t)msg.u.m3.m3l1;
    }

    return msg.RETVAL;
}
