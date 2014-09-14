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

#include "lyos/type.h"
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
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "lyos/list.h"
#include "libsysfs.h"
#include "node.h"
#include "global.h"

PRIVATE int node_num;

PUBLIC void init_node()
{
    node_num = 0;
    INIT_LIST_HEAD(&(root_node->children));
}

PUBLIC sysfs_node_t sysfs_new_node(char * name, int flags, int type)
{
    sysfs_node_t * node = (sysfs_node_t *)malloc(sizeof(sysfs_node_t));
    if (node == NULL) return NULL;

    INIT_LIST_HEAD(&(node->children));
    strcpy(node->name, name);
    node->flags = flags;
    node->type = type;

    return node;
}

PUBLIC sysfs_node_t * find_node(sysfs_node_t * parent, char * name)
{
    sysfs_node_t * node;
    list_for_each_entry(node, &(parent->children), list) {
        if (strcmp(node->name, name) == 0) {
            return node;
        }
    }

    return NULL;
}
