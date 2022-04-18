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

#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <lyos/const.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

int tmpfs_chmod(dev_t dev, ino_t num, mode_t* mode)
{
    struct tmpfs_inode* pin;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return EINVAL;

    pin->mode = (pin->mode & ~ALL_MODES) | (*mode & ALL_MODES);
    pin->update |= CTIME;

    *mode = pin->mode;

    tmpfs_put_inode(pin);

    return 0;
}
