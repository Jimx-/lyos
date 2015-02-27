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

struct memfs_hooks {
    int (*init_hook)();
    int (*message_hook)(MESSAGE * m);
};

extern struct memfs_hooks fs_hooks;

PUBLIC int memfs_start(char * name, struct memfs_hooks * hooks, struct memfs_stat * root_stat);
PUBLIC int memfs_readsuper(dev_t dev, int flags, struct fsdriver_node * node);
PUBLIC int memfs_lookup(dev_t dev, ino_t start, char * name, struct fsdriver_node * fn, int * is_mountpoint);

PUBLIC struct memfs_inode * memfs_add_inode(struct memfs_inode * parent, char * name, struct memfs_stat * stat, cbdata_t data);
#endif
