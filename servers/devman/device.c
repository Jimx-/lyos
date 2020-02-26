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
#include <stdlib.h>
#include "unistd.h"
#include <errno.h>
#include <string.h>
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
#include <libmemfs/libmemfs.h>
#include "type.h"

struct device_attr_cb_data {
    struct list_head list;
    sysfs_dyn_attr_id_t sysfs_id;
    dev_attr_id_t id;
    endpoint_t owner;
    struct device* device;
};

#define DEVICE_ATTR_HASH_LOG2 7
#define DEVICE_ATTR_HASH_SIZE ((unsigned long)1 << DEVICE_ATTR_HASH_LOG2)
#define DEVICE_ATTR_HASH_MASK (((unsigned long)1 << DEVICE_ATTR_HASH_LOG2) - 1)

/* device attribute hash table */
PRIVATE struct list_head device_attr_table[DEVICE_ATTR_HASH_SIZE];

/* device attribute show buffer size */
#define BUFSIZE 4096

#define ID2INDEX(id) (id - 1)
#define INDEX2ID(idx) (idx + 1)

PRIVATE struct device devices[NR_DEVICES];

PUBLIC void init_device()
{
    int i;
    for (i = 0; i < NR_DEVICES; i++) {
        devices[i].id = NO_DEVICE_ID;
    }

    for (i = 0; i < DEVICE_ATTR_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&device_attr_table[i]);
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

    if (retval) {
        return retval;
    }

    if (dev->devt != NO_DEV) {
        struct memfs_stat stat;
        stat.st_mode =
            ((dev->type == DT_BLOCKDEV ? I_BLOCK_SPECIAL : I_CHAR_SPECIAL) |
             0644);
        stat.st_uid = SU_UID;
        stat.st_gid = 0;
        stat.st_device = dev->devt;
        struct memfs_inode* pin = memfs_add_inode(
            memfs_get_root_inode(), dev->name, NO_INDEX, &stat, NULL);

        if (!pin) {
            return ENOMEM;
        }
    }

    return 0;
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
    dev->devt = devinf.devt;
    dev->type = devinf.type;

    if (publish_device(dev) != 0) return NO_DEVICE_ID;

    return dev->id;
}

PUBLIC struct device* get_device(device_id_t id)
{
    if (id == NO_DEVICE_ID) return NULL;

    return &devices[ID2INDEX(id)];
}

PRIVATE struct device_attr_cb_data* alloc_device_attr()
{
    static dev_attr_id_t next_id = 1;
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)malloc(sizeof(struct device_attr_cb_data));

    if (!attr) return NULL;

    attr->id = next_id++;
    INIT_LIST_HEAD(&attr->list);

    return attr;
}

PRIVATE void device_attr_addhash(struct device_attr_cb_data* attr)
{
    /* Add a device attribute to hash table */
    unsigned int hash = attr->id & DEVICE_ATTR_HASH_MASK;
    list_add(&attr->list, &device_attr_table[hash]);
}

/*
PRIVATE void device_attr_unhash(struct device_attr_cb_data* attr)
{
    \/\* Remove a dynamic attribute from hash table \*\/
    list_del(&attr->list);
}
*/

PRIVATE ssize_t device_attr_show(sysfs_dyn_attr_t* sf_attr, char* buf)
{
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)sf_attr->cb_data;

    MESSAGE msg;
    msg.type = DM_DEVICE_ATTR_SHOW;
    msg.BUF = buf;
    msg.CNT = BUFSIZE;
    msg.TARGET = attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, attr->owner, &msg);

    ssize_t count = msg.CNT;
    if (count < 0) return count;

    if (count >= BUFSIZE) return E2BIG;
    buf[count] = '\0';

    return count;
}

PRIVATE ssize_t device_attr_store(sysfs_dyn_attr_t* sf_attr, const char* buf,
                                  size_t count)
{
    struct device_attr_cb_data* attr =
        (struct device_attr_cb_data*)(sf_attr->cb_data);

    MESSAGE msg;

    msg.type = DM_DEVICE_ATTR_STORE;
    msg.BUF = (char*)buf;
    msg.CNT = count;
    msg.TARGET = attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, attr->owner, &msg);

    return msg.CNT;
}

PUBLIC int do_device_attr_add(MESSAGE* m)
{
    struct device_attr_info info;
    char device_root[MAX_PATH];
    char label[MAX_PATH];

    if (m->BUF_LEN != sizeof(info)) return EINVAL;

    data_copy(SELF, &info, m->source, m->BUF, m->BUF_LEN);

    struct device_attr_cb_data* attr = alloc_device_attr();
    attr->device = get_device(info.device);
    attr->owner = m->source;

    device_domain_label(attr->device, device_root);
    snprintf(label, MAX_PATH, "%s.%s", device_root, info.name);

    sysfs_dyn_attr_t sysfs_attr;
    int retval =
        sysfs_init_dyn_attr(&sysfs_attr, label, SF_PRIV_OVERWRITE, (void*)attr,
                            device_attr_show, device_attr_store);
    if (retval) return retval;
    retval = sysfs_publish_dyn_attr(&sysfs_attr);
    if (retval < 0) return -retval;

    attr->sysfs_id = retval;
    device_attr_addhash(attr);

    m->ATTRID = attr->id;
    return 0;
}
