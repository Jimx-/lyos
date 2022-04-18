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
#include "lyos/const.h"
#include "errno.h"
#include <sys/stat.h>

#include "ext2_fs.h"

int ext2_chmod(dev_t dev, ino_t num, mode_t* mode)
{
    ext2_inode_t* pin = get_ext2_inode(dev, num);
    if (!pin) return EINVAL;

    pin->i_mode = (pin->i_mode & ~ALL_MODES) | (*mode & ALL_MODES);
    pin->i_dirt = INO_DIRTY;
    pin->i_update |= CTIME;

    *mode = pin->i_mode;

    put_ext2_inode(pin);
    return 0;
}

int ext2_chown(dev_t dev, ino_t num, uid_t uid, gid_t gid, mode_t* mode)
{
    ext2_inode_t* pin = get_ext2_inode(dev, num);
    if (!pin) return EINVAL;

    pin->i_uid = uid;
    pin->i_gid = gid;
    pin->i_mode &= ~(S_ISUID | S_ISGID);
    pin->i_dirt = INO_DIRTY;
    pin->i_update |= CTIME;

    *mode = pin->i_mode;

    put_ext2_inode(pin);
    return 0;
}
