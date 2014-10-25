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

PRIVATE int ext2_rw_chunk(ext2_inode_t * pin, u64 position, int chunk, int left, int offset, int rw_flag, endpoint_t src, void * buf);

/**
 * Ext2 read/write syscall.
 * @param  p Ptr to message.
 * @return   Zero on success.
 */
PUBLIC int ext2_rdwt(MESSAGE * p)
{
    dev_t dev = (int)p->RWDEV;
    ino_t num = p->RWINO;
    u64 position = p->RWPOS;
    int src = p->RWSRC;
    int rw_flag = p->RWFLAG;
    void * buf = p->RWBUF;
    int nbytes = p->RWCNT;

    int retval = 0;
    ext2_inode_t * pin = find_ext2_inode(dev, num);
    if (!pin) return EINVAL;

    int file_size = pin->i_size;
    int block_size = pin->i_sb->sb_block_size;

    int bytes_rdwt = 0;

    while (nbytes > 0) {
        /* split */
        int offset = (unsigned int)position % block_size;
        int chunk = min(block_size - offset, nbytes);
        int eof = 0;

        if (rw_flag == READ) {
            if (file_size < position) break;
            if (chunk > file_size - position) { /* EOF reached */
                eof = 1;
                chunk = file_size - position;
            }
        }

        /* rw */
        retval = ext2_rw_chunk(pin, position, chunk, nbytes, offset, rw_flag, src, buf);
        if (retval) break;

        /* move on */
        nbytes -= chunk;
        position += chunk;
        bytes_rdwt += chunk;

        if (eof) break;

        buf = (void*)((int)buf + chunk);
    }

    p->RWPOS = position;

    /* update things */
    int file_type = pin->i_mode & I_TYPE;
    if (rw_flag == WRITE) {
        if (file_type == I_REGULAR || file_type == I_DIRECTORY) {
            if (position > pin->i_size) pin->i_size = position;
        }
    }

    if (retval == 0) {
        if (rw_flag == READ) {
            pin->i_update = ATIME;
        } else {
            pin->i_update = CTIME | MTIME;
        }
        pin->i_dirt = 1;
    }

    p->RWCNT = bytes_rdwt;

    return 0;
}

/**
 * Read/write a chunk of data.
 * @param  pin      The inode to read/write.
 * @param  position Where to read/write.
 * @param  chunk    How many bytes to read/write.
 * @param  left     Max number of bytes wanted.
 * @param  offset   In block offset.
 * @param  rw_flag  Read or write.
 * @param  src      Who wanna read/write.
 * @param  buf      Buffer.
 * @return          Zero on success.
 */
PRIVATE int ext2_rw_chunk(ext2_inode_t * pin, u64 position, int chunk, int left, int offset, int rw_flag, endpoint_t src, void * buf)
{

    int b = 0;
    int dev = 0;
    ext2_buffer_t * bp = NULL;

    b = ext2_read_map(pin, position);
    dev = pin->i_dev;

    if (b == 0) {
        if (rw_flag == READ) {
            /* Reading from a block that doesn't exist. Return empty buffer. */
            bp = ext2_get_buffer(0, 0);
            ext2_zero_buffer(bp);
        } else {
            /* Writing to a block that doesn't exist. Just create one. */
            bp = ext2_new_block(pin, position);
            if (bp == NULL) return err_code;
        }
    } else if (rw_flag == READ) {
        /* Read that block */
        bp = ext2_get_buffer(dev, b);
    } else if (rw_flag == WRITE) {
        /* TODO: Don't read in full block */
        bp = ext2_get_buffer(dev, b);
    }

    if (bp == NULL) {
        panic("ext2fs: ext2_rw_chunk: bp == NULL!");
    }

    if (rw_flag == READ) {
        /* copy the data to userspace */
        data_copy(src, buf, SELF, (void *)((int)bp->b_data + offset), chunk);
    } else {
        /* copy the data from userspace */
        data_copy(SELF, (void *)((int)bp->b_data + offset), src, buf, chunk);
        bp->b_dirt = 1;
    }

    ext2_put_buffer(bp);

    return 0;
}

/* locate the block number where the position can be found */
PUBLIC block_t ext2_read_map(ext2_inode_t * pin, off_t position)
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
    ext2_buffer_t * pb = NULL;

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
            pb = ext2_get_buffer(pin->i_dev, b);
            excess = block_pos - triple_ind_s;
            index = excess / addr_in_block2;
            b = *(pb->b_data + sizeof(block_t) * index);    /* num of double ind block */
            excess = excess % addr_in_block2;
            ext2_put_buffer(pb);
        }
        if (b == 0) return b;
        pb = ext2_get_buffer(pin->i_dev, b);
        index = excess / addr_in_block;
        b = *(pb->b_data + sizeof(block_t) * index);    /* num of single ind block */
        index = excess % addr_in_block; /* index into single ind blk */
    }
    if (b == 0) return b;
    pb = ext2_get_buffer(pin->i_dev, b);
    b = *(block_t *)(pb->b_data + sizeof(block_t) * index);
    return b;
}

PUBLIC int ext2_write_map(ext2_inode_t * pin, off_t position, block_t new_block)
{
    block_t b1, b2, b3;
    int index1, index2, index3;
    char new_dbl = 0, new_triple = 0;
    int single = 0, triple = 0;
    unsigned long excess, block_pos;
    static char first_time = 1;
    static long addr_in_block;
    static long addr_in_block2;
    /* double indirect slots */
    static long doub_ind_s;
    /* triple indirect slots */
    static long triple_ind_s;
    static long out_range_s;
             
    ext2_buffer_t * pb = NULL,
        *pb_dindir = NULL,
        *pb_tindir = NULL;

    if (first_time) {
        addr_in_block = pin->i_sb->sb_block_size / EXT2_BLOCK_ADDRESS_BYTES;
        addr_in_block2 = addr_in_block * addr_in_block;
        doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
        triple_ind_s = doub_ind_s + addr_in_block2;
        out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
        first_time = 0;
    }

    block_pos = position / pin->i_sb->sb_block_size; 

    if (block_pos < EXT2_NDIR_BLOCKS) {
        pin->i_block[block_pos] = new_block;
        pin->i_blocks += pin->i_sb->sb_block_size / 512;
        return 0;
    }


    if (block_pos < doub_ind_s) {
        b1 = pin->i_block[EXT2_NDIR_BLOCKS];
        index1 = block_pos - EXT2_NDIR_BLOCKS;
        single = TRUE;
    } else if (block_pos >= out_range_s) return EFBIG;
    else {
        excess = block_pos - doub_ind_s;
        b2 = pin->i_block[EXT2_DIND_BLOCK];
        if (block_pos >= triple_ind_s) {
            b3 = pin->i_block[EXT2_TIND_BLOCK];
            if (b3 == 0) {
                if ((b3 = ext2_alloc_block(pin)) == 0) {
                    return ENOSPC;
                }
                pin->i_block[EXT2_TIND_BLOCK] = b3;
                pin->i_blocks += pin->i_sb->sb_block_size / 512;
                new_triple = TRUE;
            }
            else {
                pb_tindir = ext2_get_buffer(pin->i_dev, b3);
                if (new_triple) {
                    pb_tindir->b_dirt = 1;
                }
                excess = block_pos - triple_ind_s;
                index3 = excess / addr_in_block2;
                b2 = *(block_t *)((int)pb_tindir->b_data + sizeof(block_t) * index3);
                excess = excess % addr_in_block2;
            }
            triple = TRUE;
        }

        if (b2 == 0) {
            if ( (b2 = ext2_alloc_block(pin) ) == 0) {
                ext2_put_buffer(pb_tindir);
                return(ENOSPC);
            }
            if (triple) {
                *(block_t *)((int)pb_tindir->b_data + sizeof(block_t) * index3) = b2;  /* update triple indir */
                pb_tindir->b_dirt = 1;
            } else {
                pin->i_block[EXT2_DIND_BLOCK] = b2;
            }
            pin->i_blocks += pin->i_sb->sb_block_size / 512;
            new_dbl = TRUE; /* set flag for later */
        }

        if (b2 != 0) {
            pb_dindir = ext2_get_buffer(pin->i_dev, b2);
            if (new_dbl) {
                pb_dindir->b_dirt = 1;
            }
            index2 = excess / addr_in_block;
            b1 = *(block_t *)((int)pb_dindir->b_data + sizeof(block_t) * index2);
            index1 = excess % addr_in_block;
        }
        single = FALSE;

        if (b1 == 0) {
            if ((b1 = ext2_alloc_block(pin)) == 0) {
                ext2_put_buffer(pb_dindir);
                ext2_put_buffer(pb_tindir);
                return ENOSPC;
            }   
            if (single) {
                pin->i_block[EXT2_NDIR_BLOCKS] = b1; /* update inode single indirect */
            } else {
                *(block_t *)((int)pb_dindir + sizeof(block_t) * index2) = b1;  /* update dbl indir */
                pb_dindir->b_dirt = 1;
            }
            pin->i_blocks += pin->i_sb->sb_block_size / 512;
        }

        if (b1 != 0) {
            pb = ext2_get_buffer(pin->i_dev, b1);
        } else {
            *(block_t *)((int)pb + sizeof(block_t) * index1) = new_block;
            pin->i_blocks += pin->i_sb->sb_block_size / 512;
        }
        pb->b_dirt = (b1 == 0);
        ext2_put_buffer(pb);
    }

    ext2_put_buffer(pb_dindir);
    ext2_put_buffer(pb_tindir); 

    return 0;
}

PUBLIC ext2_buffer_t * ext2_new_block(ext2_inode_t * pin, off_t position)
{
    block_t b = ext2_read_map(pin, position);

    if (b == 0) {   
        b = ext2_alloc_block(pin);

        /* can't allocate a block */
        if (b == 0) {
            err_code = ENOSPC;
            return NULL;
        }

        int ret;
        if ((ret = ext2_write_map(pin, position, b)) != 0) {
            err_code = ret;
            return NULL;
        }
    }

    return ext2_get_buffer(pin->i_dev, b);
}

