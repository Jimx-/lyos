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
#include "libsysfs/libsysfs.h"
#include "libmemfs/libmemfs.h"
#include "node.h"
#include "global.h"
#include "proto.h"

static sysfs_dyn_attr_id_t alloc_dyn_attr_id()
{
    static sysfs_dyn_attr_id_t next_id = 1;
    return next_id++;
}

int do_publish(MESSAGE* m)
{
    endpoint_t src = m->source;
    int len = m->NAME_LEN;
    int flags = m->FLAGS;

    char name[PATH_MAX];
    if (len > PATH_MAX) return ENAMETOOLONG;

    /* fetch the name */
    data_copy(SELF, name, src, m->PATHNAME, len);
    name[len] = '\0';

    sysfs_node_t* node = create_node(name, flags);
    if (!node) return errno;

    if (flags & SF_TYPE_U32) {
        node->u.u32v = m->u.m3.m3i3;
    } else if (flags & SF_TYPE_DYNAMIC) {
        node->u.dyn_attr->owner = src;
        node->u.dyn_attr->id = alloc_dyn_attr_id();
        m->ATTRID = node->u.dyn_attr->id;
    }

    return 0;
}

int do_publish_link(MESSAGE* m)
{
    endpoint_t src = m->source;
    size_t target_len, link_path_len;
    char target[PATH_MAX + 1], link_path[PATH_MAX + 1];
    sysfs_node_t *target_node, *link_node;

    target_len = m->u.m_sysfs_publish_link.target_len;
    link_path_len = m->u.m_sysfs_publish_link.link_path_len;

    if (target_len > PATH_MAX || link_path_len > PATH_MAX) return ENAMETOOLONG;

    data_copy(SELF, target, src, m->u.m_sysfs_publish_link.target, target_len);
    target[target_len] = '\0';

    data_copy(SELF, link_path, src, m->u.m_sysfs_publish_link.link_path,
              link_path_len);
    link_path[link_path_len] = '\0';

    target_node = lookup_node_by_name(target);
    if (!target_node) return errno;

    link_node = create_node(link_path, SF_TYPE_LINK | SF_PRIV_OVERWRITE);
    if (!link_node) return errno;

    link_node->link_target = target_node;

    return 0;
}

int do_retrieve(MESSAGE* m)
{
    endpoint_t src = m->source;
    int len = m->NAME_LEN;
    int flags = m->FLAGS;

    char name[PATH_MAX];
    if (len > PATH_MAX) return ENAMETOOLONG;

    /* fetch the name */
    data_copy(SELF, name, src, m->PATHNAME, len);
    name[len] = '\0';

    sysfs_node_t* node = lookup_node_by_name(name);
    if (!node) return ENOENT;

    if (node->flags & SF_PRIV_RETRIEVE) return EPERM;

    if (flags & SF_TYPE_U32) {
        if (!(node->flags & SF_TYPE_U32)) return EINVAL;
        m->u.m3.m3i3 = node->u.u32v;
    }

    return 0;
}
