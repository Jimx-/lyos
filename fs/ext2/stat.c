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
#include "stdio.h"
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"
#include <sys/stat.h>

static int ext2_stat_inode(ext2_inode_t* pin, struct fsdriver_data* data);

/**
 * Stat an inode.
 * @param  pin Ptr to the inode.
 * @param  src Who want to do stat.
 * @param  buf Buffer.
 * @return     Zero on success.
 */
static int ext2_stat_inode(ext2_inode_t* pin, struct fsdriver_data* data)
{
    struct stat sbuf;

    memset(&sbuf, 0, sizeof(struct stat));

    int special = S_ISCHR(pin->i_mode) || S_ISBLK(pin->i_mode);

    /* fill in the information */
    sbuf.st_dev = pin->i_dev;
    sbuf.st_ino = pin->i_num;
    sbuf.st_mode = pin->i_mode;
    sbuf.st_nlink = pin->i_links_count;
    sbuf.st_uid = pin->i_uid;
    sbuf.st_gid = pin->i_gid;
    sbuf.st_rdev = (special ? pin->i_block[0] : 0);
    sbuf.st_size = pin->i_size;
    sbuf.st_atime = pin->i_atime;
    sbuf.st_mtime = pin->i_mtime;
    sbuf.st_ctime = pin->i_ctime;
    sbuf.st_blksize = pin->i_sb->sb_block_size;
    sbuf.st_blocks = pin->i_blocks;

    /* copy the information */
    fsdriver_copyout(data, 0, &sbuf, sizeof(struct stat));

    return 0;
}

/**
 * Ext2 stat syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
int ext2_stat(dev_t dev, ino_t num, struct fsdriver_data* data)
{
    /* find the inode */
    ext2_inode_t* pin = get_ext2_inode(dev, num);
    if (!pin) return EINVAL;

    return ext2_stat_inode(pin, data);
}
