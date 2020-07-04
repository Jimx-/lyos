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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include <stdlib.h>
#include <errno.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "proto.h"
#include <lyos/fs.h>
#include "lyos/proc.h"
#include "lyos/driver.h"
#include <lyos/sysutils.h>
#include <libsysfs/libsysfs.h>
#include <libdevman/libdevman.h>
#include "type.h"

struct bus_attr_cb_data {
    struct list_head list;
    sysfs_dyn_attr_id_t sysfs_id;
    dev_attr_id_t id;
    struct bus_type* bus;
};

#define ID2INDEX(id) (id - 1)
#define INDEX2ID(idx) (idx + 1)

#define NR_BUS_TYPES 32
static struct bus_type bus_types[NR_BUS_TYPES];

#define BUS_ATTR_HASH_LOG2 7
#define BUS_ATTR_HASH_SIZE ((unsigned long)1 << BUS_ATTR_HASH_LOG2)
#define BUS_ATTR_HASH_MASK (((unsigned long)1 << BUS_ATTR_HASH_LOG2) - 1)

/* bus attribute hash table */
static struct list_head bus_attr_table[BUS_ATTR_HASH_SIZE];

/* bus attribute show buffer size */
#define BUFSIZE 4096

void init_bus()
{
    int i;
    for (i = 0; i < NR_BUS_TYPES; i++) {
        bus_types[i].id = BUS_TYPE_ERROR;
    }

    for (i = 0; i < BUS_ATTR_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&bus_attr_table[i]);
    }
}

static struct bus_type* alloc_bus_type()
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

void bus_domain_label(struct bus_type* bus, char* buf)
{
    snprintf(buf, MAX_PATH, SYSFS_BUS_DOMAIN_LABEL, bus->name);
}

static int publish_bus_type(struct bus_type* bus)
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

bus_type_id_t do_bus_register(MESSAGE* m)
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

struct bus_type* get_bus_type(bus_type_id_t id)
{
    if (id == BUS_TYPE_ERROR) return NULL;

    int i = ID2INDEX(id);
    return &bus_types[i];
}

static struct bus_attr_cb_data* alloc_bus_attr()
{
    static dev_attr_id_t next_id = 1;
    struct bus_attr_cb_data* attr =
        (struct bus_attr_cb_data*)malloc(sizeof(struct bus_attr_cb_data));

    if (!attr) return NULL;

    attr->id = next_id++;
    INIT_LIST_HEAD(&attr->list);

    return attr;
}

static void bus_attr_addhash(struct bus_attr_cb_data* attr)
{
    /* Add a bus attribute to hash table */
    unsigned int hash = attr->id & BUS_ATTR_HASH_MASK;
    list_add(&attr->list, &bus_attr_table[hash]);
}

/*
static void bus_attr_unhash(struct bus_attr_cb_data* attr)
{
    \/\* Remove a dynamic attribute from hash table \*\/
    list_del(&attr->list);
}
*/

static ssize_t bus_attr_show(sysfs_dyn_attr_t* sf_attr, char* buf)
{
    struct bus_attr_cb_data* attr = (struct bus_attr_cb_data*)sf_attr->cb_data;

    MESSAGE msg;
    msg.type = DM_BUS_ATTR_SHOW;
    msg.BUF = buf;
    msg.CNT = BUFSIZE;
    msg.TARGET = attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, attr->bus->owner, &msg);

    ssize_t count = msg.CNT;
    if (count < 0) return count;

    if (count >= BUFSIZE) return E2BIG;
    buf[count] = '\0';

    return count;
}

static ssize_t bus_attr_store(sysfs_dyn_attr_t* sf_attr, const char* buf,
                              size_t count)
{
    struct bus_attr_cb_data* attr =
        (struct bus_attr_cb_data*)(sf_attr->cb_data);

    MESSAGE msg;

    msg.type = DM_BUS_ATTR_STORE;
    msg.BUF = (char*)buf;
    msg.CNT = count;
    msg.TARGET = attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, attr->bus->owner, &msg);

    return msg.CNT;
}

int do_bus_attr_add(MESSAGE* m)
{
    struct bus_attr_info info;
    char bus_root[MAX_PATH - BUS_NAME_MAX - 1];
    char label[MAX_PATH];

    if (m->BUF_LEN != sizeof(info)) return EINVAL;

    data_copy(SELF, &info, m->source, m->BUF, m->BUF_LEN);

    struct bus_attr_cb_data* attr = alloc_bus_attr();
    attr->bus = get_bus_type(info.bus);

    bus_domain_label(attr->bus, bus_root);
    snprintf(label, MAX_PATH, "%s.%s", bus_root, info.name);

    sysfs_dyn_attr_t sysfs_attr;
    int retval =
        sysfs_init_dyn_attr(&sysfs_attr, label, SF_PRIV_OVERWRITE, (void*)attr,
                            bus_attr_show, bus_attr_store);
    if (retval) return retval;
    retval = sysfs_publish_dyn_attr(&sysfs_attr);
    if (retval < 0) return -retval;

    attr->sysfs_id = retval;
    bus_attr_addhash(attr);

    m->ATTRID = attr->id;
    return 0;
}
