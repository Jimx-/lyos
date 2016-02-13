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

#include "lyos/type.h"
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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

PRIVATE ext2_inode_t * ext2_new_inode(ext2_inode_t * pin_dir, char * pathname, mode_t mode, block_t initial_block, uid_t uid, gid_t gid);

/**
 * <Ring 1> Create a file.
 * @param p Ptr to the message.
 * @return  Zero on success.
 */
PUBLIC int ext2_create(dev_t dev, ino_t dir_num, char * name, mode_t mode, uid_t uid, gid_t gid, struct fsdriver_node * fn)
{
    ext2_inode_t * pin_dir = get_ext2_inode(dev, dir_num);
    if (pin_dir == NULL) return ENOENT;

    ext2_inode_t * pin = ext2_new_inode(pin_dir, name, mode, 0, uid, gid);
    int retval = err_code;

    if (pin == NULL) {
        //put_ext2_inode(pin);
        put_ext2_inode(pin_dir);
        return retval;
    } 

    fn->fn_num = pin->i_num;
    fn->fn_mode = pin->i_mode;
    fn->fn_size = pin->i_size;
    fn->fn_uid = pin->i_uid;
    fn->fn_gid = pin->i_gid;

    put_ext2_inode(pin_dir);
    return 0;
}

/**
 * Create an inode.
 * @param  pin_dir  The directory on which the inode resides.
 * @param  pathname The pathname.
 * @param  mode     Inode mode.
 * @param  b        Block to start searching.
 * @return          Zero on success.
 */
PRIVATE ext2_inode_t * ext2_new_inode(ext2_inode_t * pin_dir, char * pathname, mode_t mode, block_t initial_block, uid_t uid, gid_t gid)
{
    /* the directory does not actually exist */
    if (pin_dir->i_links_count == 0) {
        err_code = ENOENT;
        return NULL;
    }
    
    /* try to get the last component */
    ext2_inode_t * pin = ext2_advance(pin_dir, pathname);
    
    if (pin == NULL && err_code == ENOENT) {
        pin = ext2_alloc_inode(pin_dir, mode);
        if (pin == NULL) return NULL;
    }

    pin->i_uid = uid;
    pin->i_gid = gid;
    pin->i_links_count++;
    pin->i_block[0] = initial_block;
    /* update all times */
    pin->i_update = ATIME | CTIME | MTIME;

    ext2_rw_inode(pin, BDEV_WRITE);

    /* create the directory entry */
    int retval = ext2_search_dir(pin_dir, pathname, &pin->i_num, SD_MAKE,
              pin->i_mode & I_TYPE);

    if (retval != 0) {
        pin->i_links_count--;
        pin->i_dirt = 1;
        put_ext2_inode(pin);
        err_code = retval;
        return NULL;
    }

    if (err_code == EENTERMOUNT || err_code == ELEAVEMOUNT) {
        retval = EEXIST;
    }

    err_code = retval;
    return pin;
}
