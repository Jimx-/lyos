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

#include <lyos/type.h>
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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

ssize_t ext2_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                    size_t bytes)
{
    ssize_t retval = 0;
    ext2_inode_t* pin = find_ext2_inode(dev, num);
    char* text = NULL;
    struct fsd_buffer* bp = NULL;

    if (!pin) return -EINVAL;

    if (pin->i_size >= EXT2_MAX_FAST_SYMLINK_LENGTH) {
        block_t b = ext2_read_map(pin, 0);
        retval = fsd_get_block(&bp, dev, b);

        if (retval) {
            return -retval;
        }

        text = bp->data;
    } else {
        text = (char*)pin->i_block;
    }

    if (bytes >= pin->i_size) bytes = pin->i_size;
    retval = fsdriver_copyout(data, 0, text, bytes);
    if (bp) fsd_put_block(bp);

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
