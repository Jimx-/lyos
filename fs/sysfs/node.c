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
#include <stdlib.h>
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
#include <sys/syslimits.h>
#include "node.h"
#include "proto.h"
#include "global.h"

static int node_num;

void init_node()
{
    node_num = 0;

    memset(root_node.name, 0, sizeof(root_node.name));
    root_node.flags = SF_TYPE_DOMAIN;
    root_node.inode = memfs_get_root_inode();
    INIT_LIST_HEAD(&(root_node.list));
    INIT_LIST_HEAD(&(root_node.children));
}

sysfs_node_t* new_node(char* name, int flags)
{
    sysfs_node_t* node = (sysfs_node_t*)malloc(sizeof(sysfs_node_t));
    if (node == NULL) return NULL;

    INIT_LIST_HEAD(&(node->list));
    INIT_LIST_HEAD(&(node->children));
    strlcpy(node->name, name, NAME_MAX);
    node->flags = flags;
    node->parent = NULL;
    node->link_target = NULL;

    switch (NODE_TYPE(node)) {
    case SF_TYPE_DYNAMIC:
        node->u.dyn_attr = (dyn_attr_info_t*)malloc(sizeof(dyn_attr_info_t));
        break;
    default:
        break;
    }

    return node;
}

int free_node(sysfs_node_t* node)
{
    switch (NODE_TYPE(node)) {
    case SF_TYPE_DYNAMIC:
        if (node->u.dyn_attr) free(node->u.dyn_attr);
        break;
    default:
        break;
    }

    free(node);

    return 0;
}

sysfs_node_t* find_node(sysfs_node_t* parent, char* name)
{
    sysfs_node_t* node;

    if (NODE_TYPE(parent) != SF_TYPE_DOMAIN) return NULL;

    list_for_each_entry(node, &(parent->children), list)
    {
        if (strcmp(node->name, name) == 0) {
            return node;
        }
    }

    return NULL;
}

sysfs_node_t* lookup_node_by_name(char* name)
{
    sysfs_node_t* dir_pn = &root_node;

    char component[NAME_MAX];
    char* end;

    while (*name != '\0') {
        end = name;
        while (*end != '\0' && *end != '.')
            end++;

        int len = end - name;
        if (len == 0) {
            name++;
            continue;
        }

        memcpy(component, name, len);
        component[len] = '\0';

        sysfs_node_t* pn = find_node(dir_pn, component);
        if (!pn) {
            errno = ENOENT;
            return NULL;
        }
        dir_pn = pn;

        if (*end == '\0') break;
        name = end + 1;
    }

    return dir_pn;
}

sysfs_node_t* create_node(char* name, int flags)
{
    if (name == NULL) return NULL;

    char* end = name + strlen(name);

    while (*end != '.' && end > name)
        end--;
    if (name == end)
        name = "\0";
    else
        *end++ = '\0';

    sysfs_node_t* dir_pn = lookup_node_by_name(name);
    if (!dir_pn) {
        errno = ENOENT;
        return NULL;
    }

    if (find_node(dir_pn, end) != NULL) {
        errno = EEXIST;
        return NULL;
    }

    sysfs_node_t* new_pn = new_node(end, flags);
    if (!new_pn) {
        errno = ENOMEM;
        return NULL;
    }

    errno = add_node(dir_pn, new_pn);
    if (errno) {
        free_node(new_pn);
        return NULL;
    }

    return new_pn;
}

static void stat_node(struct memfs_stat* stat, sysfs_node_t* node)
{
    int file_type = 0;
    mode_t bits;

    switch (NODE_TYPE(node)) {
    case SF_TYPE_DOMAIN:
        bits = 0755;
        file_type = S_IFDIR;
        break;
    case SF_TYPE_LINK:
        bits = 0777;
        file_type = S_IFLNK;
        break;
    default:
        bits = 0644;
        file_type = S_IFREG;
        break;
    }

    stat->st_mode = (file_type | bits);
    stat->st_uid = SU_UID;
    stat->st_gid = 0;
}

int add_node(sysfs_node_t* parent, sysfs_node_t* child)
{
    struct memfs_stat stat;
    stat_node(&stat, child);

    struct memfs_inode* pin =
        memfs_add_inode(parent->inode, child->name, NO_INDEX, &stat, child);
    if (!pin) return ENOMEM;

    list_add(&child->list, &parent->children);

    child->parent = parent;
    child->inode = pin;

    return 0;
}
