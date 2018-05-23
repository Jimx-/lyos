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
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"

PRIVATE int memfs_stat_inode(struct memfs_inode* pin, struct fsdriver_data* data);

/**
 * Stat an inode.
 * @param  pin Ptr to the inode.
 * @param  src Who want to do stat.
 * @param  buf Buffer.
 * @return     Zero on success.
 */
PRIVATE int memfs_stat_inode(struct memfs_inode* pin, struct fsdriver_data* data)
{
    struct stat sbuf;

    memset(&sbuf, 0, sizeof(struct stat));

    int mode = pin->i_stat.st_mode & I_TYPE;
    int special = (mode == I_CHAR_SPECIAL) || (mode == I_BLOCK_SPECIAL);

    /* fill in the information */
    sbuf.st_dev = pin->i_stat.st_dev;
    sbuf.st_ino = pin->i_num;
    sbuf.st_mode = pin->i_stat.st_mode;
    sbuf.st_nlink = 1;
    sbuf.st_atime = pin->i_atime;
    sbuf.st_ctime = pin->i_ctime;
    sbuf.st_mtime = pin->i_mtime;
    sbuf.st_uid = pin->i_stat.st_uid;
    sbuf.st_gid = pin->i_stat.st_gid;
    sbuf.st_rdev = (special ? pin->i_stat.st_device : 0);
    sbuf.st_size = pin->i_stat.st_size;

    /* copy the information */
    fsdriver_copyout(data, 0, &sbuf, sizeof(struct stat));

    return 0;
}

/**
 * memfs stat syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
PUBLIC int memfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data)
{
    /* find the inode */
    struct memfs_inode* pin = memfs_find_inode(num);
    if (!pin) return EINVAL;

    return memfs_stat_inode(pin, data);
}
