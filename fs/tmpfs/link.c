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
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <asm/page.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

int tmpfs_link(dev_t dev, ino_t dir_num, const char* name, ino_t num)
{
    struct tmpfs_inode *dir_pin, *pin, *tmp;
    int retval;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) return EINVAL;

    retval = EINVAL;
    pin = tmpfs_get_inode(dev, num);
    if (!pin) goto put_dir_pin;

    retval = EPERM;
    if (S_ISDIR(pin->mode)) goto put_pin;

    retval = tmpfs_advance(dir_pin, name, &tmp);
    if (retval == OK) {
        tmpfs_put_inode(tmp);
        retval = EEXIST;
        goto put_pin;
    } else if (retval == ENOENT)
        retval = 0;

    if (retval == OK) retval = tmpfs_search_dir(dir_pin, name, &pin, SD_MAKE);

    if (retval == OK) {
        pin->link_count++;
        pin->update |= CTIME;
    }

put_pin:
    tmpfs_put_inode(pin);
put_dir_pin:
    tmpfs_put_inode(dir_pin);

    return retval;
}

static int unlink_file(struct tmpfs_inode* dir_pin, struct tmpfs_inode* pin,
                       const char* name)
{
    int retval;

    if (!pin) {
        retval = tmpfs_search_dir(dir_pin, name, &pin, SD_LOOK_UP);
        if (retval) return retval;
    }

    tmpfs_dup_inode(pin);

    retval = tmpfs_search_dir(dir_pin, name, NULL, SD_DELETE);

    if (!retval) {
        pin->link_count--;
        pin->update |= CTIME;
    }

    tmpfs_put_inode(pin);

    return retval;
}

static int remove_dir(struct tmpfs_inode* dir_pin, struct tmpfs_inode* pin,
                      const char* name)
{
    int retval;

    if ((retval = tmpfs_search_dir(pin, "", NULL, SD_IS_EMPTY)) != OK)
        return retval;

    if (pin->num == TMPFS_ROOT_INODE) return EBUSY;

    if ((retval = unlink_file(dir_pin, pin, name)) != OK) return retval;

    unlink_file(pin, NULL, ".");
    unlink_file(pin, NULL, "..");

    return 0;
}

static int do_unlink(dev_t dev, ino_t dir_num, const char* name, int is_rmdir)
{
    struct tmpfs_inode *dir_pin, *pin;
    int retval;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) return EINVAL;

    retval = tmpfs_advance(dir_pin, name, &pin);
    if (retval) {
        tmpfs_put_inode(dir_pin);
        return retval;
    }

    if (pin->mountpoint) {
        tmpfs_put_inode(pin);
        tmpfs_put_inode(dir_pin);
        return EBUSY;
    }

    if (is_rmdir) {
        retval = remove_dir(dir_pin, pin, name);
    } else {
        if (S_ISDIR(pin->mode)) retval = EPERM;

        if (retval == OK) retval = unlink_file(dir_pin, pin, name);
    }

    tmpfs_put_inode(pin);
    tmpfs_put_inode(dir_pin);

    return retval;
}

int tmpfs_unlink(dev_t dev, ino_t dir_num, const char* name)
{
    return do_unlink(dev, dir_num, name, FALSE /* is_rmdir */);
}

int tmpfs_rmdir(dev_t dev, ino_t dir_num, const char* name)
{
    return do_unlink(dev, dir_num, name, TRUE /* is_rmdir */);
}

ssize_t tmpfs_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                     size_t bytes, endpoint_t user_endpt)
{
    struct tmpfs_inode* pin;
    char* page;
    ssize_t retval = 0;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return -EINVAL;

    retval = tmpfs_inode_getpage(pin, 0, &page);
    if (retval) {
        retval = -retval;
        goto out;
    }

    if (bytes >= pin->size) bytes = pin->size;
    retval = fsdriver_copyout(data, 0, page, bytes);

    if (retval == 0) {
        retval = bytes;
    } else {
        retval = -retval;
    }

out:
    tmpfs_put_inode(pin);

    return retval;
}

int tmpfs_symlink(dev_t dev, ino_t dir_num, const char* name, uid_t uid,
                  gid_t gid, struct fsdriver_data* data, size_t bytes)
{
    struct tmpfs_inode *pin, *dir_pin;
    char* page = NULL;
    int retval;

    dir_pin = tmpfs_get_inode(dev, dir_num);
    if (!dir_pin) {
        return EINVAL;
    }

    retval =
        tmpfs_new_inode(dir_pin, name, S_IFLNK | RWX_MODES, uid, gid, &pin);

    if (retval == OK) {
        if (bytes >= ARCH_PG_SIZE)
            retval = ENAMETOOLONG;
        else {
            retval = tmpfs_inode_getpage(pin, 0, &page);

            if (retval == OK) {
                retval = fsdriver_copyin(data, 0, page, bytes);
            }
        }

        if (retval == OK) {
            page[bytes] = '\0';
            pin->size = strlen(page);

            if (pin->size != bytes) retval = ENAMETOOLONG;
        }
    }

    tmpfs_put_inode(pin);
    tmpfs_put_inode(dir_pin);

    return retval;
}

int tmpfs_ftrunc(dev_t dev, ino_t num, off_t start_pos, off_t end_pos)
{
    struct tmpfs_inode* pin;
    int retval = ENOSYS;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return EINVAL;

    if (end_pos == 0)
        retval = tmpfs_truncate_inode(pin, start_pos);
    else
        retval = tmpfs_free_range(pin, start_pos, end_pos);

    tmpfs_put_inode(pin);
    return retval;
}
