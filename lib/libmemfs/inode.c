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

#include <lyos/type.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include "libmemfs/libmemfs.h"

PUBLIC struct memfs_inode root_inode;

PUBLIC struct list_head memfs_inode_table[MEMFS_INODE_HASH_SIZE];

PRIVATE ino_t allocate_inode_num()
{
    static ino_t num = 1;
    return num++;
}

PUBLIC void memfs_init_inode()
{
    int i;
    for (i = 0; i < MEMFS_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&memfs_inode_table[i]);
    }
}

PUBLIC void memfs_addhash_inode(struct memfs_inode * inode)
{
    unsigned int hash = inode->i_num & MEMFS_INODE_HASH_MASK;

    list_add(&(inode->i_hash), &memfs_inode_table[hash]);
}

PRIVATE void memfs_unhash_inode(struct memfs_inode * inode)
{
    list_del(&(inode->i_hash));
}

PUBLIC struct memfs_inode * memfs_new_inode(ino_t num, char * name)
{
    struct memfs_inode * pin = (struct memfs_inode *)malloc(sizeof(struct memfs_inode));
    if (!pin) return NULL;

    pin->i_num = num;
    strcpy(pin->i_name, name);
    pin->i_name[strlen(name)] = '\0';
    INIT_LIST_HEAD(&pin->i_hash);
    INIT_LIST_HEAD(&pin->i_list);
    INIT_LIST_HEAD(&pin->i_children);

    memfs_addhash_inode(pin);

    return pin;
}

PUBLIC struct memfs_inode * memfs_find_inode(ino_t num)
{
    int hash = num & MEMFS_INODE_HASH_MASK;

    struct memfs_inode * inode;
    list_for_each_entry(inode, &memfs_inode_table[hash], i_hash) {
        if (inode->i_num == num) {   /* hit */
            return inode;
        }
    }

    return NULL;
}

PUBLIC struct memfs_inode * memfs_get_root_inode()
{
    return &root_inode;
}

PUBLIC void memfs_set_inode_stat(struct memfs_inode * pin, struct memfs_stat * stat)
{
    if (!pin || !stat) return;

    memcpy(&pin->i_stat, stat, sizeof(struct memfs_stat));
}

PUBLIC struct memfs_inode * memfs_add_inode(struct memfs_inode * parent, char * name, struct memfs_stat * stat, cbdata_t data)
{
    ino_t new_num = allocate_inode_num();

    struct memfs_inode * pin = memfs_new_inode(new_num, name);
    if (!pin) return NULL;

    list_add(&pin->i_list, &parent->i_children);
    pin->data = data;
    memfs_set_inode_stat(pin, stat);

    return pin;
}