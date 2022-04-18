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
#include <errno.h>
#include <lyos/const.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <sys/stat.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "proto.h"
#include "types.h"

static int tmpfs_mountpoint(dev_t dev, ino_t num);

struct fsdriver tmpfs_driver = {
    .name = "tmpfs",
    .root_num = TMPFS_ROOT_INODE,

    .fs_readsuper = tmpfs_readsuper,
    .fs_mountpoint = tmpfs_mountpoint,
    .fs_putinode = tmpfs_putinode,
    .fs_lookup = tmpfs_lookup,
    .fs_create = tmpfs_create,
    .fs_mkdir = tmpfs_mkdir,
    .fs_mknod = tmpfs_mknod,
    .fs_read = tmpfs_read,
    .fs_write = tmpfs_write,
    .fs_rdlink = tmpfs_rdlink,
    .fs_unlink = tmpfs_unlink,
    .fs_rmdir = tmpfs_rmdir,
    .fs_link = tmpfs_link,
    .fs_symlink = tmpfs_symlink,
    .fs_stat = tmpfs_stat,
    .fs_chmod = tmpfs_chmod,
    .fs_getdents = tmpfs_getdents,
    .fs_utime = tmpfs_utime,
    .fs_ftrunc = tmpfs_ftrunc,
};

static int tmpfs_mountpoint(dev_t dev, ino_t num)
{
    struct tmpfs_inode* pin;
    int retval = 0;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return EINVAL;

    if (pin->mountpoint) return EBUSY;

    if (!S_ISDIR(pin->mode)) retval = ENOTDIR;

    if (retval == OK) pin->mountpoint = 1;

    tmpfs_put_inode(pin);

    return retval;
}

static int init_tmpfs(void)
{
    printl("tmpfs: Tmpfs driver is running.\n");

    init_inode();

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(init_tmpfs);
    serv_init();

    return fsdriver_start(&tmpfs_driver);
}
