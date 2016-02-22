/*  
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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "proto.h"
#include <lyos/fs.h>
#include "lyos/proc.h"
#include "lyos/driver.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include <libsysfs/libsysfs.h>
#include <libdevman/libdevman.h>
#include "type.h"

#define ID2INDEX(id) (id - 1)
#define INDEX2ID(idx) (idx + 1)

PRIVATE struct device devices[NR_DEVICES];

PUBLIC void init_device()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        devices[i].id = NO_DEVICE_ID;
    }
}

PRIVATE struct device* alloc_device()
{
    int i;
    struct device* dev = NULL;

    for (i = 0; i < NR_DEVICES; i++) {
        if (devices[i].id == NO_DEVICE_ID) {
            dev = &devices[i];
            break;
        }
    }

    if (!dev) return NULL;

    dev->id = INDEX2ID(i);
    memset(dev->name, 0, sizeof(dev->name));

    return dev;
}


PRIVATE void device_domain_label(struct device* dev, char* buf)
{
    char name[MAX_PATH];

    snprintf(name, MAX_PATH, "%s", dev->name);
    dev = dev->parent;

    int len = strlen(name) + 1;
    while (dev) {
        size_t name_len = strlen(dev->name);
        memmove(name + name_len + 1, name, len);
        len += name_len + 1;
        strcpy(name, dev->name);
        name[name_len] = '.';
        dev = dev->parent;
    }

    snprintf(buf, MAX_PATH, "devices.%s", name);
}

PRIVATE int publish_device(struct device* dev)
{
    char label[MAX_PATH];
    device_domain_label(dev, label);
    int retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);

    return retval;
}

PUBLIC device_id_t do_device_register(MESSAGE* m)
{
    struct device_info devinf;

    if (m->BUF_LEN != sizeof(devinf)) return NO_DEVICE_ID;

    data_copy(SELF, &devinf, m->source, m->BUF, m->BUF_LEN);

    struct device* dev = alloc_device();
    if (!dev) return NO_DEVICE_ID;

    strlcpy(dev->name, devinf.name, sizeof(dev->name));
    dev->owner = m->source;
    dev->bus = get_bus_type(devinf.bus);
    dev->parent = get_device(devinf.parent);

    if (publish_device(dev) != 0) return NO_DEVICE_ID;

    return dev->id;
}

PUBLIC struct device* get_device(device_id_t id)
{
    if (id == NO_DEVICE_ID) return NULL;

    return &devices[ID2INDEX(id)];
}
