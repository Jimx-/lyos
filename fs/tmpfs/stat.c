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
#include <lyos/config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/list.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <sys/stat.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

static int tmpfs_stat_inode(struct tmpfs_inode* pin, struct fsdriver_data* data)
{
    struct stat sbuf;
    int special;

    memset(&sbuf, 0, sizeof(struct stat));

    special = S_ISCHR(pin->mode) || S_ISBLK(pin->mode);

    /* fill in the information */
    sbuf.st_dev = pin->dev;
    sbuf.st_ino = pin->num;
    sbuf.st_mode = pin->mode;
    sbuf.st_nlink = pin->link_count;
    sbuf.st_atime = pin->atime;
    sbuf.st_ctime = pin->ctime;
    sbuf.st_mtime = pin->mtime;
    sbuf.st_uid = pin->uid;
    sbuf.st_gid = pin->gid;
    sbuf.st_rdev = (special ? pin->spec_dev : 0);
    sbuf.st_size = pin->size;

    /* copy the information */
    fsdriver_copyout(data, 0, &sbuf, sizeof(struct stat));

    return 0;
}

int tmpfs_stat(dev_t dev, ino_t num, struct fsdriver_data* data)
{
    struct tmpfs_inode* pin;
    int retval;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return EINVAL;

    retval = tmpfs_stat_inode(pin, data);

    tmpfs_put_inode(pin);
    return retval;
}
