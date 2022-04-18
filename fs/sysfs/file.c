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
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include <libsysfs/libsysfs.h>
#include "node.h"
#include "proto.h"

#define BUFSIZE 4096

static ssize_t dyn_attr_show(sysfs_node_t* node)
{
    static char buf[BUFSIZE];

    MESSAGE msg;
    msg.type = SYSFS_DYN_SHOW;
    msg.BUF = buf;
    msg.CNT = BUFSIZE;
    msg.TARGET = node->u.dyn_attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, node->u.dyn_attr->owner, &msg);

    ssize_t count = msg.CNT;
    if (count < 0) return count;

    if (count >= BUFSIZE) return E2BIG;
    buf[count] = '\0';
    buf_printf("%s", buf);

    return count;
}

static ssize_t dyn_attr_store(sysfs_node_t* node, const char* ptr, size_t count)
{
    MESSAGE msg;
    msg.type = SYSFS_DYN_STORE;
    msg.BUF = (char*)ptr;
    msg.CNT = count;
    msg.TARGET = node->u.dyn_attr->id;

    /* TODO: async read/write */
    send_recv(BOTH, node->u.dyn_attr->owner, &msg);

    return msg.CNT;
}

ssize_t sysfs_read_hook(struct memfs_inode* inode, char* ptr, size_t count,
                        off_t offset, cbdata_t data)
{
    init_buf(ptr, count, offset);

    sysfs_node_t* node = (sysfs_node_t*)data;
    int retval = 0;

    switch (NODE_TYPE(node)) {
    case SF_TYPE_U32:
        buf_printf("%d\n", node->u.u32v);
        break;
    case SF_TYPE_DYNAMIC:
        retval = dyn_attr_show(node);
        break;
    }
    if (retval < 0) return retval;

    return buf_used();
}

ssize_t sysfs_write_hook(struct memfs_inode* inode, char* ptr, size_t count,
                         off_t offset, cbdata_t data)
{
    sysfs_node_t* node = (sysfs_node_t*)data;
    int retval = 0;

    switch (NODE_TYPE(node)) {
    case SF_TYPE_DYNAMIC:
        retval = dyn_attr_store(node, ptr, count);
        break;
    }

    return retval;
}

static ssize_t get_target_path(struct memfs_inode* parent,
                               struct memfs_inode* target, char* path,
                               size_t max)
{
    struct memfs_inode *base, *pin;
    char* p = path;
    size_t len = 0, plen;

    base = parent;
    if (base) {
        while (memfs_node_parent(base)) {
            pin = memfs_node_parent(target);

            if (pin) {
                while (memfs_node_parent(pin) && base != pin) {
                    pin = memfs_node_parent(pin);
                }
            }

            if (base == pin) {
                break;
            }

            if ((p - path + 3) >= max) {
                return -ENAMETOOLONG;
            }

            strcpy(p, "../");
            p += 3;
            base = memfs_node_parent(base);
        }
    }

    pin = target;
    while (memfs_node_parent(pin) && pin != base) {
        len += strlen(pin->i_name) + 1;
        pin = memfs_node_parent(pin);
    }

    if (len < 2) {
        return -EINVAL;
    }
    len--;

    plen = (p - path) + len;
    if (plen >= max) {
        return -ENAMETOOLONG;
    }

    pin = target;
    while (memfs_node_parent(pin) && pin != base) {
        size_t slen = strlen(pin->i_name);
        len -= slen;

        memcpy(p + len, pin->i_name, slen);
        if (len) p[--len] = '/';

        pin = memfs_node_parent(pin);
    }

    path[plen] = '\0';

    return plen;
}

int sysfs_rdlink_hook(struct memfs_inode* inode, char* ptr, size_t max,
                      endpoint_t user_endpt, cbdata_t data)
{
    sysfs_node_t* node = (sysfs_node_t*)data;
    struct memfs_inode* target;
    ssize_t retval;

    if (NODE_TYPE(node) != SF_TYPE_LINK) {
        return EINVAL;
    }

    target = node->link_target->inode;

    retval = get_target_path(memfs_node_parent(inode), target, ptr, max);
    if (retval < 0) return -retval;

    return 0;
}
