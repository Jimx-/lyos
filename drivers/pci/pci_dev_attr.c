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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include <lyos/portio.h>
#include <lyos/service.h>
#include <libsysfs/libsysfs.h>

#include <asm/pci.h>
#include "pci.h"

ssize_t pci_vendor_show(struct device_attribute* attr, char* buf)
{
    struct pcidev* dev = (struct pcidev*)(attr->cb_data);
    return sprintf(buf, "0x%04x\n", dev->vid);
}

ssize_t pci_device_show(struct device_attribute* attr, char* buf)
{
    struct pcidev* dev = (struct pcidev*)(attr->cb_data);
    return sprintf(buf, "0x%04x\n", dev->did);
}

ssize_t pci_class_show(struct device_attribute* attr, char* buf)
{
    struct pcidev* dev = (struct pcidev*)(attr->cb_data);
    return sprintf(buf, "0x%02x%02x%02x\n", dev->baseclass, dev->subclass,
                   dev->infclass);
}
