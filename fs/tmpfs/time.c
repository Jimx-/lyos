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

int tmpfs_utime(dev_t dev, ino_t num, const struct timespec* atime,
                const struct timespec* mtime)
{
    struct tmpfs_inode* pin;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return EINVAL;

    pin->update = CTIME;

    switch (atime->tv_nsec) {
    case UTIME_NOW:
        pin->update |= ATIME;
        break;
    case UTIME_OMIT:
        break;
    default:
        pin->atime = atime->tv_sec;
        break;
    }

    switch (mtime->tv_nsec) {
    case UTIME_NOW:
        pin->update |= MTIME;
        break;
    case UTIME_OMIT:
        break;
    default:
        pin->mtime = mtime->tv_sec;
        break;
    }

    tmpfs_put_inode(pin);

    return 0;
}
