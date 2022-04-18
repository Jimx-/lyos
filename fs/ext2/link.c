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
#include "lyos/const.h"
#include "errno.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/list.h"
#include <sys/stat.h>

#include "ext2_fs.h"
#include "global.h"

static int unlink_file(ext2_inode_t* pin_dir, ext2_inode_t* pin,
                       const char* name)
{
    int retval;
    ino_t num;

    if (!pin) {
        retval = ext2_search_dir(pin_dir, name, &num, SD_LOOK_UP, 0);
        if (!retval)
            pin = get_ext2_inode(pin_dir->i_dev, num);
        else
            return retval;

        if (!pin) return err_code;
    } else {
        pin->i_count++;
    }

    retval = ext2_search_dir(pin_dir, name, NULL, SD_DELETE, 0);

    if (!retval) {
        pin->i_links_count--;
        pin->i_update |= CTIME;
        pin->i_dirt = INO_DIRTY;
    }

    put_ext2_inode(pin);
    return retval;
}

static int remove_dir(ext2_inode_t* pin_dir, ext2_inode_t* pin,
                      const char* name)
{
    int retval;

    if ((retval = ext2_search_dir(pin, "", NULL, SD_IS_EMPTY, 0)) != OK)
        return retval;

    if (pin->i_num == EXT2_ROOT_INODE) return EBUSY;

    if ((retval = unlink_file(pin_dir, pin, name)) != OK) return retval;

    unlink_file(pin, NULL, ".");
    unlink_file(pin, NULL, "..");
    return OK;
}

static int do_unlink(dev_t dev, ino_t dir_num, const char* name, int is_rmdir)
{
    ext2_inode_t *pin_dir, *pin;
    int retval = OK;

    pin_dir = get_ext2_inode(dev, dir_num);
    if (!pin_dir) return EINVAL;

    pin = ext2_advance(pin_dir, name);
    if (!pin) {
        put_ext2_inode(pin_dir);
        return err_code;
    }

    if (pin->i_mountpoint) {
        put_ext2_inode(pin);
        put_ext2_inode(pin_dir);
        return EBUSY;
    }

    if (is_rmdir) {
        retval = remove_dir(pin_dir, pin, name);
    } else {
        if (S_ISDIR(pin->i_mode)) {
            retval = EPERM;
        }

        if (retval == OK) retval = unlink_file(pin_dir, pin, name);
    }

    put_ext2_inode(pin);
    put_ext2_inode(pin_dir);
    return retval;
}

int ext2_unlink(dev_t dev, ino_t dir_num, const char* name)
{
    return do_unlink(dev, dir_num, name, FALSE /* is_rmdir */);
}

int ext2_rmdir(dev_t dev, ino_t dir_num, const char* name)
{
    return do_unlink(dev, dir_num, name, TRUE /* is_rmdir */);
}

ssize_t ext2_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                    size_t bytes, endpoint_t user_endpt)
{
    ssize_t retval = 0;
    ext2_inode_t* pin = get_ext2_inode(dev, num);
    char* text = NULL;
    struct fsdriver_buffer* bp = NULL;

    if (!pin) return -EINVAL;

    if (pin->i_size >= EXT2_MAX_FAST_SYMLINK_LENGTH) {
        block_t b = ext2_read_map(pin, 0);
        retval = fsdriver_get_block(&bp, dev, b);

        if (retval) {
            return -retval;
        }

        text = bp->data;
    } else {
        text = (char*)pin->i_block;
    }

    if (bytes >= pin->i_size) bytes = pin->i_size;
    retval = fsdriver_copyout(data, 0, text, bytes);
    if (bp) fsdriver_put_block(bp);

    if (retval == 0) {
        retval = bytes;
    } else {
        retval = -retval;
    }

    put_ext2_inode(pin);

    return retval;
}

/**
 * <Ring 1> Perform the FTRUNC syscall.
 * @param  p Ptr to the message.
 * @return   Zero on success.
 */
int ext2_ftrunc(dev_t dev, ino_t num, off_t start_pos, off_t end_pos)
{
    return 0;
}
