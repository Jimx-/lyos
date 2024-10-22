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

#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>

#include <libasyncdriver/libasyncdriver.h>
#include <libdevman/libdevman.h>

#define BUFSIZE 4096

#define DEVICE_ATTR_HASH_LOG2 7
#define DEVICE_ATTR_HASH_SIZE ((unsigned long)1 << DEVICE_ATTR_HASH_LOG2)
#define DEVICE_ATTR_HASH_MASK (((unsigned long)1 << DEVICE_ATTR_HASH_LOG2) - 1)

/* device attribute hash table */
static struct list_head device_attr_table[DEVICE_ATTR_HASH_SIZE];

int dm_init_device_attr(struct device_attribute* attr, device_id_t device,
                        char* name, mode_t mode, void* cb_data,
                        device_attr_show_t show, device_attr_store_t store)
{
    static int initialized = 0;
    if (!initialized) {
        int i;
        for (i = 0; i < DEVICE_ATTR_HASH_SIZE; i++) {
            INIT_LIST_HEAD(&device_attr_table[i]);
        }
        initialized = 1;
    }

    if (strlen(name) >= ATTR_NAME_MAX) {
        return ENAMETOOLONG;
    }
    strlcpy(attr->info.name, name, ATTR_NAME_MAX);
    attr->info.name[ATTR_NAME_MAX - 1] = '\0';
    attr->info.device = device;
    attr->info.mode = mode;

    attr->cb_data = cb_data;
    attr->show = show;
    attr->store = store;
    INIT_LIST_HEAD(&attr->list);

    return 0;
}

static void device_attr_addhash(struct device_attribute* attr)
{
    /* Add a device attribute to hash table */
    unsigned int hash = attr->id & DEVICE_ATTR_HASH_MASK;
    list_add(&attr->list, &device_attr_table[hash]);
}

/*
static void device_attr_unhash(struct device_attribute* attr)
{
    list_del(&attr->list);
}
*/

static struct device_attribute* find_device_attr_by_id(dev_attr_id_t id)
{
    /* Find a dynamic attribute by its attribute id */
    unsigned int hash = id & DEVICE_ATTR_HASH_MASK;

    struct device_attribute* attr;
    list_for_each_entry(attr, &device_attr_table[hash], list)
    {
        if (attr->id == id) {
            return attr;
        }
    }

    return NULL;
}

int dm_device_attr_add(struct device_attribute* attr)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_ATTR_ADD;
    msg.BUF = &attr->info;
    msg.BUF_LEN = sizeof(attr->info);

    send_recv(BOTH, TASK_DEVMAN, &msg);

    if (msg.RETVAL) return msg.RETVAL;

    struct device_attribute* new_attr =
        (struct device_attribute*)malloc(sizeof(struct device_attribute));
    if (!new_attr) return ENOMEM;
    memcpy(new_attr, attr, sizeof(*attr));

    dev_attr_id_t id = (dev_attr_id_t)msg.ATTRID;
    new_attr->id = id;

    device_attr_addhash(new_attr);

    return 0;
}

int dm_async_device_attr_add(struct device_attribute* attr)
{
    MESSAGE msg;

    msg.type = DM_DEVICE_ATTR_ADD;
    msg.BUF = &attr->info;
    msg.BUF_LEN = sizeof(attr->info);
    msg.u.m3.m3l1 = asyncdrv_worker_id();

    asyncdrv_sendrec(TASK_DEVMAN, &msg);

    if (msg.RETVAL) return msg.RETVAL;

    struct device_attribute* new_attr =
        (struct device_attribute*)malloc(sizeof(struct device_attribute));
    if (!new_attr) return ENOMEM;
    memcpy(new_attr, attr, sizeof(*attr));

    dev_attr_id_t id = (dev_attr_id_t)msg.ATTRID;
    new_attr->id = id;

    device_attr_addhash(new_attr);

    return 0;
}

void dm_device_attr_handle(MESSAGE* msg)
{
    /* handle device attribute show/store request */
    int rw_flag = msg->type;
    static char tmp_buf[BUFSIZE];
    size_t count = msg->u.m_devman_attr_req.len;
    char* buf = msg->u.m_devman_attr_req.buf;
    dev_attr_id_t target = msg->u.m_devman_attr_req.target;
    ssize_t retval;

    struct device_attribute* attr = find_device_attr_by_id(target);
    if (!attr) {
        retval = -ENOENT;
        goto reply;
    }

    if (rw_flag == DM_DEVICE_ATTR_SHOW) {
        if (!attr->show) {
            retval = -EPERM;
            goto reply;
        }

        retval = attr->show(attr, tmp_buf, sizeof(tmp_buf));
        if (retval >= count) {
            retval = -E2BIG;
            goto reply;
        }
        if (retval < 0) goto reply;

        data_copy(msg->source, buf, SELF, tmp_buf, retval);
    } else {
        if (!attr->store) {
            retval = -EPERM;
            goto reply;
        }

        if (count >= BUFSIZE) {
            retval = -E2BIG;
            goto reply;
        }

        data_copy(SELF, tmp_buf, msg->source, buf, count);

        retval = attr->store(attr, tmp_buf, count);
    }

reply:
    msg->u.m_devman_attr_reply.opaque = msg->u.m_devman_attr_req.opaque;
    if (retval < 0) {
        msg->u.m_devman_attr_reply.status = -retval;
        msg->u.m_devman_attr_reply.count = 0;
    } else {
        msg->u.m_devman_attr_reply.status = 0;
        msg->u.m_devman_attr_reply.count = retval;
    }

    msg->type = DM_DEVICE_ATTR_REPLY;
    send_recv(SEND_NONBLOCK, msg->source, msg);
}
