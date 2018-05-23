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
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include "libmemfs/libmemfs.h"

PUBLIC struct memfs_hooks fs_hooks;

PRIVATE int memfs_other(MESSAGE * m);

struct fsdriver fsd = {
    .name = NULL,
    .root_num = 0,

    .fs_readsuper = memfs_readsuper,
    .fs_lookup = memfs_lookup,
    .fs_stat = memfs_stat,
    .fs_readwrite = memfs_readwrite,
    .fs_getdents = memfs_getdents,
    .fs_other = memfs_other,
};

PUBLIC int memfs_start(char * name, struct memfs_hooks * hooks, struct memfs_stat * root_stat)
{
    fsd.name = name;

    if (!hooks) return EINVAL;
    memcpy(&fs_hooks, hooks, sizeof(fs_hooks));
    
    if (memfs_init_buf()) return ENOMEM;

    memfs_init_inode();

    root_inode.i_num = 0;
    root_inode.i_index = NO_INDEX;
    root_inode.i_parent = NULL;
    INIT_LIST_HEAD(&root_inode.i_hash);
    INIT_LIST_HEAD(&root_inode.i_list);
    INIT_LIST_HEAD(&root_inode.i_children);
    memfs_set_inode_stat(&root_inode, root_stat);
    memfs_addhash_inode(&root_inode);

    if (fs_hooks.init_hook) fs_hooks.init_hook();
    
    return fsdriver_start(&fsd);
}

PRIVATE int memfs_other(MESSAGE * m)
{
    if (fs_hooks.message_hook) return fs_hooks.message_hook(m);
    else return ENOSYS;
}
