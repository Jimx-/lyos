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
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>

#include "libmemfs/libmemfs.h"
#include "proto.h"

struct memfs_hooks fs_hooks;

static void memfs_other(MESSAGE* m);

#define MEMFS_ROOT_INODE 1

static struct fsdriver fsd = {
    .name = NULL,
    .root_num = MEMFS_ROOT_INODE,

    .fs_readsuper = memfs_readsuper,
    .fs_lookup = memfs_lookup,
    .fs_mkdir = memfs_mkdir,
    .fs_mknod = memfs_mknod,
    .fs_stat = memfs_stat,
    .fs_read = memfs_read,
    .fs_write = memfs_write,
    .fs_getdents = memfs_getdents,
    .fs_rdlink = memfs_rdlink,
    .fs_other = memfs_other,
};

int memfs_start(char* name, struct memfs_hooks* hooks,
                struct memfs_stat* root_stat)
{
    struct memfs_inode* root = memfs_get_root_inode();

    fsd.name = name;

    if (!hooks) return EINVAL;
    memcpy(&fs_hooks, hooks, sizeof(fs_hooks));

    if (memfs_init_buf()) return ENOMEM;

    memfs_init_inode();

    root->i_num = MEMFS_ROOT_INODE;
    root->i_index = NO_INDEX;
    root->i_parent = NULL;
    INIT_LIST_HEAD(&root->i_hash);
    INIT_LIST_HEAD(&root->i_list);
    INIT_LIST_HEAD(&root->i_children);
    memfs_set_inode_stat(root, root_stat);
    memfs_addhash_inode(root);

    if (fs_hooks.init_hook) fs_hooks.init_hook();

    return fsdriver_start(&fsd);
}

static void memfs_other(MESSAGE* m)
{
    if (fs_hooks.message_hook) fs_hooks.message_hook(m);
}
