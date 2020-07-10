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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/driver.h>
#include <lyos/proto.h>
#include <libdevman/libdevman.h>
#include <libsysfs/libsysfs.h>

#define BUFSIZE 4096

#define BUS_ATTR_HASH_LOG2 7
#define BUS_ATTR_HASH_SIZE ((unsigned long)1 << BUS_ATTR_HASH_LOG2)
#define BUS_ATTR_HASH_MASK (((unsigned long)1 << BUS_ATTR_HASH_LOG2) - 1)

/* bus attribute hash table */
static struct list_head bus_attr_table[BUS_ATTR_HASH_SIZE];

int dm_init_bus_attr(struct bus_attribute* attr, bus_type_id_t bus, char* name,
                     mode_t mode, void* cb_data, bus_attr_show_t show,
                     bus_attr_store_t store)
{
    static int initialized = 0;
    if (!initialized) {
        int i;
        for (i = 0; i < BUS_ATTR_HASH_SIZE; i++) {
            INIT_LIST_HEAD(&bus_attr_table[i]);
        }
        initialized = 1;
    }

    if (strlen(name) >= ATTR_NAME_MAX) {
        return ENAMETOOLONG;
    }
    strlcpy(attr->info.name, name, ATTR_NAME_MAX);
    attr->info.name[ATTR_NAME_MAX - 1] = '\0';
    attr->info.bus = bus;
    attr->info.mode = mode;

    attr->cb_data = cb_data;
    attr->show = show;
    attr->store = store;
    INIT_LIST_HEAD(&attr->list);

    return 0;
}

static void bus_attr_addhash(struct bus_attribute* attr)
{
    unsigned int hash = attr->id & BUS_ATTR_HASH_MASK;
    list_add(&attr->list, &bus_attr_table[hash]);
}

/*
static void bus_attr_unhash(struct bus_attribute* attr)
{
    list_del(&attr->list);
}
*/

static struct bus_attribute* find_bus_attr_by_id(dev_attr_id_t id)
{
    /* Find a dynamic attribute by its attribute id */
    unsigned int hash = id & BUS_ATTR_HASH_MASK;

    struct bus_attribute* attr;
    list_for_each_entry(attr, &bus_attr_table[hash], list)
    {
        if (attr->id == id) {
            return attr;
        }
    }

    return NULL;
}

int dm_bus_attr_add(struct bus_attribute* attr)
{
    MESSAGE msg;

    msg.type = DM_BUS_ATTR_ADD;
    msg.BUF = &attr->info;
    msg.BUF_LEN = sizeof(attr->info);

    send_recv(BOTH, TASK_DEVMAN, &msg);

    if (msg.RETVAL) return msg.RETVAL;

    struct bus_attribute* new_attr =
        (struct bus_attribute*)malloc(sizeof(struct bus_attribute));
    if (!new_attr) return ENOMEM;
    memcpy(new_attr, attr, sizeof(*attr));

    dev_attr_id_t id = (dev_attr_id_t)msg.ATTRID;
    new_attr->id = id;

    bus_attr_addhash(new_attr);

    return 0;
}

ssize_t dm_bus_attr_handle(MESSAGE* msg)
{
    /* handle bus attribute show/store request */
    int rw_flag = msg->type;
    static char tmp_buf[BUFSIZE];
    ssize_t count = msg->CNT;
    char* buf = msg->BUF;

    struct bus_attribute* attr = find_bus_attr_by_id(msg->TARGET);
    if (!attr) return -ENOENT;

    if (rw_flag == DM_BUS_ATTR_SHOW) {
        if (!attr->show) return -EPERM;

        ssize_t byte_read = attr->show(attr, tmp_buf);
        if (byte_read >= count) {
            return -E2BIG;
        }
        if (byte_read < 0) return byte_read;

        data_copy(msg->source, buf, SELF, tmp_buf, byte_read);
        return byte_read;
    } else {
        if (!attr->store) return -EPERM;

        if (count >= BUFSIZE) return -E2BIG;
        data_copy(SELF, tmp_buf, msg->source, buf, count);

        return attr->store(attr, tmp_buf, count);
    }

    return 0;
}
