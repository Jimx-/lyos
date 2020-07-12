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
#include <lyos/service.h>
#include "libsysfs/libsysfs.h"

endpoint_t __pci_endpoint =
#ifdef __i386__
    TASK_PCI;
#else
    NO_TASK;
#endif

int pci_sendrec(int function, MESSAGE* msg)
{
    int retval;
    u32 v;

    if (__pci_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.pci.endpoint", &v);
        if (retval) return retval;

        __pci_endpoint = (endpoint_t)v;
    }

    return send_recv(function, __pci_endpoint, msg);
}

int pci_set_acl(struct pci_acl* pci_acl)
{
    MESSAGE msg;

    msg.type = PCI_SET_ACL;
    msg.BUF = pci_acl;

    pci_sendrec(BOTH, &msg);

    return msg.RETVAL;
}
