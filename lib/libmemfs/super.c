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

PRIVATE int mounted = 0;

PUBLIC int memfs_readsuper(dev_t dev, int flags, struct fsdriver_node * node)
{
    if (mounted) return EBUSY;
    
	node->fn_num = root_inode.i_num;
    node->fn_mode = root_inode.i_stat.st_mode;
    node->fn_size = root_inode.i_stat.st_size;
    node->fn_uid = root_inode.i_stat.st_uid;
    node->fn_gid = root_inode.i_stat.st_gid;
    node->fn_device = 0;

    mounted = 1;
    return 0;
}
