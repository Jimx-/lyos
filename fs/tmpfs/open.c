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
#include <stddef.h>
#include <lyos/const.h>
#include <sys/dirent.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

int tmpfs_create(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                 uid_t uid, gid_t gid, struct fsdriver_node* fn)
{
    struct tmpfs_inode *dir_pin, *pin;
    int retval;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) return EINVAL;

    retval = tmpfs_new_inode(dir_pin, name, mode, uid, gid, &pin);

    if (retval == OK) {
        fn->fn_num = pin->num;
        fn->fn_mode = pin->mode;
        fn->fn_size = pin->size;
        fn->fn_uid = pin->uid;
        fn->fn_gid = pin->gid;
    } else {
        if (pin) tmpfs_put_inode(pin);
    }

    tmpfs_put_inode(dir_pin);

    return retval;
}

int tmpfs_mkdir(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid)
{
    struct tmpfs_inode *dir_pin, *pin;
    int retval, r1, r2;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) return EINVAL;

    retval = tmpfs_new_inode(dir_pin, name, mode, uid, gid, &pin);
    if (retval != OK) {
        if (pin) tmpfs_put_inode(pin);
        tmpfs_put_inode(dir_pin);
        return retval;
    }

    r1 = tmpfs_search_dir(pin, ".", &pin, SD_MAKE);
    r2 = tmpfs_search_dir(pin, "..", &dir_pin, SD_MAKE);

    if (!r1 && !r2) {
        pin->link_count++;
        dir_pin->link_count++;
    } else {
        tmpfs_search_dir(dir_pin, name, NULL, SD_DELETE);
        pin->link_count--;
    }

    tmpfs_put_inode(pin);
    tmpfs_put_inode(dir_pin);

    return r1 != OK ? r1 : r2;
}

int tmpfs_mknod(dev_t dev, ino_t dir_num, const char* name, mode_t mode,
                uid_t uid, gid_t gid, dev_t sdev)
{
    struct tmpfs_inode *dir_pin, *pin;
    int retval;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) return EINVAL;

    retval = tmpfs_new_inode(dir_pin, name, mode, uid, gid, &pin);
    if (retval == OK) pin->spec_dev = sdev;

    if (pin) tmpfs_put_inode(pin);
    tmpfs_put_inode(dir_pin);

    return retval;
}
