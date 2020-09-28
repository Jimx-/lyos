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

#ifndef _LIBMEMFS_INODE_H_
#define _LIBMEMFS_INODE_H_

#include <sys/syslimits.h>

typedef void* cbdata_t;

#define ATIME 002 /* set if atime field needs updating */
#define CTIME 004 /* set if ctime field needs updating */
#define MTIME 010 /* set if mtime field needs updating */

struct memfs_stat {
    dev_t st_dev;
    dev_t st_device;
    uid_t st_uid;
    gid_t st_gid;
    size_t st_size;
    mode_t st_mode;
};

struct memfs_inode {
    ino_t i_num;
    int i_index;
    char i_name[NAME_MAX];

    struct memfs_stat i_stat;
    struct memfs_inode* i_parent;
    cbdata_t data;

    u32 i_atime;
    u32 i_mtime;
    u32 i_ctime;
    u32 i_update;

    struct list_head i_hash;
    struct list_head i_list;
    struct list_head i_children;
};

#define MEMFS_INODE_HASH_LOG2 7
#define MEMFS_INODE_HASH_SIZE ((unsigned long)1 << MEMFS_INODE_HASH_LOG2)
#define MEMFS_INODE_HASH_MASK (((unsigned long)1 << MEMFS_INODE_HASH_LOG2) - 1)

/* inode hash table */
extern struct list_head memfs_inode_table[MEMFS_INODE_HASH_SIZE];

extern struct memfs_inode root_inode;

void memfs_init_inode();
struct memfs_inode* memfs_new_inode(ino_t num, const char* name, int index);
void memfs_set_inode_stat(struct memfs_inode* pin, struct memfs_stat* stat);
struct memfs_inode* memfs_get_root_inode();
struct memfs_inode* memfs_find_inode(ino_t num);
struct memfs_inode* memfs_find_inode_by_name(struct memfs_inode* parent,
                                             const char* name);
struct memfs_inode* memfs_find_inode_by_index(struct memfs_inode* parent,
                                              int index);
void memfs_addhash_inode(struct memfs_inode* inode);

#endif
