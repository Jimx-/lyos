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
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include "proto.h"

#define MEMFS_INODE_HASH_LOG2 7
#define MEMFS_INODE_HASH_SIZE ((unsigned long)1 << MEMFS_INODE_HASH_LOG2)
#define MEMFS_INODE_HASH_MASK (((unsigned long)1 << MEMFS_INODE_HASH_LOG2) - 1)

static struct memfs_inode root_inode;

static struct list_head memfs_inode_table[MEMFS_INODE_HASH_SIZE];

static ino_t allocate_inode_num()
{
    static ino_t num = 10;
    return num++;
}

void memfs_init_inode()
{
    int i;
    for (i = 0; i < MEMFS_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&memfs_inode_table[i]);
    }
}

void memfs_addhash_inode(struct memfs_inode* inode)
{
    unsigned int hash = inode->i_num & MEMFS_INODE_HASH_MASK;

    list_add(&(inode->i_hash), &memfs_inode_table[hash]);
}

/*
static void memfs_unhash_inode(struct memfs_inode * inode)
{
    list_del(&(inode->i_hash));
}
*/

struct memfs_inode* memfs_new_inode(ino_t num, const char* name, int index)
{
    struct memfs_inode* pin =
        (struct memfs_inode*)malloc(sizeof(struct memfs_inode));
    if (!pin) return NULL;

    memset(pin, 0, sizeof(*pin));
    pin->i_num = num;
    pin->i_index = index;
    strlcpy(pin->i_name, name, sizeof(pin->i_name));
    INIT_LIST_HEAD(&pin->i_hash);
    INIT_LIST_HEAD(&pin->i_list);
    INIT_LIST_HEAD(&pin->i_children);

    memfs_addhash_inode(pin);

    return pin;
}

struct memfs_inode* memfs_find_inode(ino_t num)
{
    int hash = num & MEMFS_INODE_HASH_MASK;

    struct memfs_inode* inode;
    list_for_each_entry(inode, &memfs_inode_table[hash], i_hash)
    {
        if (inode->i_num == num) { /* hit */
            return inode;
        }
    }

    return NULL;
}

struct memfs_inode* memfs_find_inode_by_name(struct memfs_inode* parent,
                                             const char* name)
{
    struct memfs_inode* node;

    list_for_each_entry(node, &(parent->i_children), i_list)
    {
        if (strcmp(node->i_name, name) == 0) {
            return node;
        }
    }

    return NULL;
}

struct memfs_inode* memfs_find_inode_by_index(struct memfs_inode* parent,
                                              int index)
{
    struct memfs_inode* node;

    if (!S_ISDIR(parent->i_stat.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    list_for_each_entry(node, &(parent->i_children), i_list)
    {
        if (node->i_index == index) return node;
    }

    return NULL;
}

struct memfs_inode* memfs_node_parent(struct memfs_inode* pin)
{
    return pin->i_parent;
}

int memfs_node_index(struct memfs_inode* pin) { return pin->i_index; }

void memfs_update_inode(struct memfs_inode* pin)
{
    if (pin->i_update == 0) return;

    u32 timestamp = now();

    if (pin->i_update & CTIME) pin->i_ctime = timestamp;
    if (pin->i_update & MTIME) pin->i_mtime = timestamp;
    if (pin->i_update & ATIME) pin->i_atime = timestamp;
    pin->i_update = 0;
}

struct memfs_inode* memfs_get_root_inode() { return &root_inode; }

void memfs_get_inode_stat(struct memfs_inode* pin, struct memfs_stat* stat)
{
    if (!pin || !stat) return;

    *stat = pin->i_stat;
}

void memfs_set_inode_stat(struct memfs_inode* pin, struct memfs_stat* stat)
{
    if (!pin || !stat) return;

    pin->i_stat = *stat;
}

struct memfs_inode* memfs_add_inode(struct memfs_inode* parent,
                                    const char* name, int index,
                                    struct memfs_stat* stat, cbdata_t data)
{
    ino_t new_num = allocate_inode_num();

    struct memfs_inode* pin = memfs_new_inode(new_num, name, index);
    if (!pin) return NULL;

    list_add(&pin->i_list, &parent->i_children);

    memfs_set_inode_stat(pin, stat);

    pin->data = data;
    pin->i_parent = parent;

    pin->i_update = ATIME | CTIME | MTIME;
    memfs_update_inode(pin);

    return pin;
}
