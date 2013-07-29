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
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

/* locate the block number where the position can be found */
PUBLIC block_t read_map(ext2_inode_t * pin, off_t position)
{
    int index;
    block_t b;
    unsigned long excess, block_pos;
    static char first_time = 1;
    static long addr_in_block;
    static long addr_in_block2;
    /* double indirect slots */
    static long doub_ind_s;
    /* triple indirect slots */
    static long triple_ind_s;
    static long out_range_s;

    if (first_time) {
        addr_in_block = pin->i_sb->sb_block_size / EXT2_BLOCK_ADDRESS_BYTES;
        addr_in_block2 = addr_in_block * addr_in_block;
        doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
        triple_ind_s = doub_ind_s + addr_in_block2;
        out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
        first_time = 0;
    }

    block_pos = position / pin->i_sb->sb_block_size; 

    /* block number is in the inode */
    if (block_pos < EXT2_NDIR_BLOCKS)
        return(pin->i_block[block_pos]);

    /* single indirect */
    if (block_pos < doub_ind_s) {
        b = pin->i_block[EXT2_NDIR_BLOCKS]; /* address of single indirect block */
        index = block_pos - EXT2_NDIR_BLOCKS;
    } else if (block_pos >= out_range_s) { 
        return 0;
    } else {
        /* double or triple indirect block. At first if it's triple,
         * find double indirect block.
         */
        excess = block_pos - doub_ind_s;
        b = pin->i_block[EXT2_DIND_BLOCK];
        if (block_pos >= triple_ind_s) {
            b = pin->i_block[EXT2_TIND_BLOCK];
            if (b == 0) return b;
            rw_ext2_blocks(DEV_READ, pin->i_dev, b, 1, ext2fsbuf);
            excess = block_pos - triple_ind_s;
            index = excess / addr_in_block2;
            b = *(ext2fsbuf + sizeof(block_t) * index);    /* num of double ind block */
            excess = excess % addr_in_block2;
        }
        if (b == 0) return b;
        rw_ext2_blocks(DEV_READ, pin->i_dev, b, 1, ext2fsbuf);
        index = excess / addr_in_block;
        b = *(ext2fsbuf + sizeof(block_t) * index);    /* num of single ind block */
        index = excess % addr_in_block; /* index into single ind blk */
    }
    if (b == 0) return b;
    rw_ext2_blocks(DEV_READ, pin->i_dev, b, 1, ext2fsbuf);
    b = *(ext2fsbuf + sizeof(block_t) * index);

    return b;
}