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

#ifndef _LIBMEMFS_H_
#define _LIBMEMFS_H_

#include <lyos/list.h>
#include "libfsdriver/libfsdriver.h"

#include "inode.h"

#define NO_INDEX -1

struct memfs_hooks {
    int (*init_hook)();
    void (*message_hook)(MESSAGE* m);
    ssize_t (*read_hook)(struct memfs_inode* inode, char* ptr, size_t count,
                         off_t offset, cbdata_t data);
    ssize_t (*write_hook)(struct memfs_inode* inode, char* ptr, size_t count,
                          off_t offset, cbdata_t data);
    ssize_t (*getdents_hook)(struct memfs_inode* inode, cbdata_t data);
    int (*lookup_hook)(struct memfs_inode* parent, char* name, cbdata_t data);
};

extern struct memfs_hooks fs_hooks;

extern char* memfs_buf;
extern size_t memfs_bufsize;

int memfs_start(char* name, struct memfs_hooks* hooks,
                struct memfs_stat* root_stat);
int memfs_readsuper(dev_t dev, int flags, struct fsdriver_node* node);
int memfs_lookup(dev_t dev, ino_t start, char* name, struct fsdriver_node* fn,
                 int* is_mountpoint);
int memfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data);
ssize_t memfs_readwrite(dev_t dev, ino_t num, int rw_flag,
                        struct fsdriver_data* data, loff_t rwpos, size_t count);
ssize_t memfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                       loff_t* ppos, size_t count);

int memfs_init_buf();
int memfs_free_buf();

int memfs_node_index(struct memfs_inode* pin);
struct memfs_inode* memfs_node_parent(struct memfs_inode* pin);
struct memfs_inode* memfs_add_inode(struct memfs_inode* parent, char* name,
                                    int index, struct memfs_stat* stat,
                                    cbdata_t data);
#endif
