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
#include <lyos/service.h>
#include <fcntl.h>

#include "libfsdriver/libfsdriver.h"
#include "ext2_fs.h"
#include "global.h"

#define DEBUG
#ifdef DEBUG
#define DEB(x)          \
    printl("ext2fs: "); \
    x
#else
#define DEB(x)
#endif

#define BLOCK2SECTOR(blk_size, blk_nr) (blk_nr * (blk_size))

int init_ext2fs();

static int ext2_mountpoint(dev_t dev, ino_t num);

struct fsdriver ext2fsdriver = {
    .name = "ext2",
    .root_num = EXT2_ROOT_INODE,

    .fs_readsuper = ext2_readsuper,
    .fs_mountpoint = ext2_mountpoint,
    .fs_putinode = ext2_putinode,
    .fs_lookup = ext2_lookup,
    .fs_create = ext2_create,
    .fs_mkdir = ext2_mkdir,
    .fs_mknod = ext2_mknod,
    .fs_read = ext2_read,
    .fs_write = ext2_write,
    .fs_rdlink = ext2_rdlink,
    .fs_unlink = ext2_unlink,
    .fs_rmdir = ext2_rmdir,
    .fs_symlink = ext2_symlink,
    .fs_stat = ext2_stat,
    .fs_ftrunc = ext2_ftrunc,
    .fs_chmod = ext2_chmod,
    .fs_getdents = ext2_getdents,
    .fs_driver = fsdriver_driver,
    .fs_sync = ext2_sync,
};

/*****************************************************************************
 *                                task_ext2_fs
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK Ext2 FS.
 *
 *****************************************************************************/
int main()
{
    serv_register_init_fresh_callback(init_ext2fs);
    serv_init();

    return fsdriver_start(&ext2fsdriver);
}

int init_ext2fs()
{
    printl("ext2fs: Ext2 filesystem driver is running\n");

    ext2_init_inode();
    fsdriver_init_buffer_cache(1024);

    err_code = 0;

    return 0;
}

static int ext2_mountpoint(dev_t dev, ino_t num)
{
    ext2_inode_t* pin = get_ext2_inode(dev, num);

    if (pin == NULL) return EINVAL;

    if (pin->i_mountpoint) return EBUSY;

    int retval = 0;
    if (!S_ISDIR(pin->i_mode)) retval = ENOTDIR;

    if (!retval) pin->i_mountpoint = 1;
    put_ext2_inode(pin);

    return retval;
}

int ext2_sync()
{
    ext2_sync_inodes();
    fsdriver_flush_all();

    return 0;
}
