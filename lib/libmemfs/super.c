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
#include <sys/stat.h>

#include "libmemfs/libmemfs.h"
#include "proto.h"

static int mounted = 0;

int memfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                    struct fsdriver_node* node)
{
    struct memfs_inode* root = memfs_get_root_inode();

    if (mounted) return EBUSY;

    node->fn_num = root->i_num;
    node->fn_mode = root->i_stat.st_mode;
    node->fn_size = root->i_stat.st_size;
    node->fn_uid = root->i_stat.st_uid;
    node->fn_gid = root->i_stat.st_gid;
    node->fn_device = 0;

    mounted = 1;
    return 0;
}

int memfs_mountpoint(dev_t dev, ino_t num)
{
    struct memfs_inode* pin = memfs_find_inode(num);
    if (!pin) return EINVAL;

    if (pin->is_mountpoint) return EBUSY;

    if (S_ISBLK(pin->i_stat.st_mode) || S_ISCHR(pin->i_stat.st_mode))
        return ENOTDIR;

    pin->is_mountpoint = TRUE;

    return 0;
}
