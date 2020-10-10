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
#include "sys/types.h"
#include "lyos/config.h"
#include "errno.h"
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
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
        buf_printf("%d", node->u.u32v);
        break;
    case SF_TYPE_DYNAMIC:
        retval = dyn_attr_show(node);
        break;
    }
    if (retval < 0) return retval;

    buf_printf("\n");
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

int sysfs_rdlink_hook(struct memfs_inode* inode, cbdata_t data,
                      struct memfs_inode** target, endpoint_t user_endpt)
{
    sysfs_node_t* node = (sysfs_node_t*)data;

    if (NODE_TYPE(node) != SF_TYPE_LINK) {
        return EINVAL;
    }

    *target = node->link_target->inode;
    return 0;
}
