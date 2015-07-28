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

#define NR_BUS_TYPES    32
PRIVATE struct bus_type bus_types[NR_BUS_TYPES];

PUBLIC void init_bus()
{
    int i;
    for (i = 0; i < NR_BUS_TYPES; i++) {
        bus_types[i].id = BUS_TYPE_ERROR;
    }
}

PRIVATE struct bus_type* alloc_bus_type()
{
    int i;
    struct bus_type* type = NULL;

    for (i = 0; i < NR_BUS_TYPES; i++) {
        if (bus_types[i].id == BUS_TYPE_ERROR) {
            type = &bus_types[i];
            break;
        }
    }

    if (!type) return NULL;

    type->id = INDEX2ID(i);
    memset(type->name, 0, sizeof(type->name));

    return type;
}

PRIVATE int publish_bus_type(struct bus_type* bus)
{
    char label[MAX_PATH];

    char* name = bus->name;
    snprintf(label, MAX_PATH, SYSFS_BUS_DOMAIN_LABEL, name);
    int retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);
    if (retval) return retval;

    snprintf(label, MAX_PATH, SYSFS_BUS_DOMAIN_LABEL ".drivers", name);
    retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);
    if (retval) return retval;

    snprintf(label, MAX_PATH, SYSFS_BUS_DOMAIN_LABEL ".devices", name);
    retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);

    return retval;
}

PUBLIC bus_type_id_t do_bus_register(MESSAGE* m)
{
    char name[BUS_NAME_MAX];

    if (m->NAME_LEN >= BUS_NAME_MAX) return ENAMETOOLONG;

    data_copy(SELF, name, m->source, m->BUF, m->NAME_LEN);
    name[m->NAME_LEN] = '\0';

    struct bus_type* bus = alloc_bus_type();
    if (!bus) return BUS_TYPE_ERROR;

    strlcpy(bus->name, name, sizeof(bus->name));
    bus->owner = m->source;

    if (publish_bus_type(bus) != 0) return BUS_TYPE_ERROR;

    return bus->id;
}

PUBLIC struct bus_type* get_bus_type(bus_type_id_t id)
{
    if (id == BUS_TYPE_ERROR) return NULL;

    int i = ID2INDEX(id);
    return &bus_types[i];
}
