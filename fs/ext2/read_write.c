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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include <sys/dirent.h>
#include "ext2_fs.h"
#include "global.h"

static int ext2_rw_chunk(ext2_inode_t* pin, loff_t position, size_t chunk,
                         size_t left, off_t offset, int rw_flag,
                         struct fsdriver_data* data, off_t data_offset);

/**
 * Ext2 read/write syscall.
 * @param  p Ptr to message.
 * @return   Zero on success.
 */
ssize_t ext2_rdwt(dev_t dev, ino_t num, int rw_flag,
                  struct fsdriver_data* data, loff_t rwpos, size_t count)
{
    loff_t position = rwpos;
    size_t nbytes = count;
    off_t data_offset = 0;

    ssize_t retval = 0;
    ext2_inode_t* pin = find_ext2_inode(dev, num);
    if (!pin) return -EINVAL;

    size_t file_size = pin->i_size;
    size_t block_size = pin->i_sb->sb_block_size;

    ssize_t bytes_rdwt = 0;

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
        retval = ext2_rw_chunk(pin, position, chunk, nbytes, offset, rw_flag,
                               data, data_offset);
        if (retval) break;

        /* move on */
        nbytes -= chunk;
        position += chunk;
        bytes_rdwt += chunk;

        if (eof) break;

        data_offset += chunk;
    }

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

        return bytes_rdwt;
    }

    return -retval;
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
static int ext2_rw_chunk(ext2_inode_t* pin, loff_t position, size_t chunk,
                         size_t left, off_t offset, int rw_flag,
                         struct fsdriver_data* data, off_t data_offset)
{
    block_t b = 0;
    dev_t dev = 0;
    struct fsdriver_buffer* bp;
    int retval;

    b = ext2_read_map(pin, position);
    dev = pin->i_dev;

    retval = 0;
    if (b == NO_BLOCK) {
        if (rw_flag == READ) {
            /* Reading from a block that doesn't exist. Return empty buffer. */
            bp = NULL;
        } else {
            /* Writing to a block that doesn't exist. Just create one. */
            bp = ext2_new_block(pin, position);
            if (bp == NULL) return err_code;
        }
    } else if (rw_flag == READ) {
        /* Read that block */
        retval = fsdriver_get_block(&bp, dev, b);
    } else if (rw_flag == WRITE) {
        /* TODO: Don't read in full block */
        retval = fsdriver_get_block(&bp, dev, b);
    }

    if (retval) return retval;

    if (bp == NULL && b != NO_BLOCK) {
        panic("ext2fs: ext2_rw_chunk: bp == NULL!");
    }

    if (rw_flag == READ) {
        /* copy the data to userspace */
        if (b != 0)
            fsdriver_copyout(data, data_offset, (char*)bp->data + offset,
                              chunk);
    } else {
        /* copy the data from userspace */
        fsdriver_copyin(data, data_offset, (char*)bp->data + offset, chunk);
        fsdriver_mark_dirty(bp);
    }

    if (bp != NULL) fsdriver_put_block(bp);

    return 0;
}

/* locate the block number where the position can be found */
block_t ext2_read_map(ext2_inode_t* pin, loff_t position)
{
    int index, retval;
    block_t b;
    unsigned long excess, block_pos;
    static int first_time = TRUE;
    static loff_t addr_in_block;
    static loff_t addr_in_block2;
    /* double indirect slots */
    static loff_t doub_ind_s;
    /* triple indirect slots */
    static loff_t triple_ind_s;
    static loff_t out_range_s;
    struct fsdriver_buffer* bp;

    if (first_time) {
        addr_in_block = pin->i_sb->sb_block_size / EXT2_BLOCK_ADDRESS_BYTES;
        addr_in_block2 = addr_in_block * addr_in_block;
        doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
        triple_ind_s = doub_ind_s + addr_in_block2;
        out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
        first_time = FALSE;
    }

    block_pos = position / pin->i_sb->sb_block_size;

    /* block number is in the inode */
    if (block_pos < EXT2_NDIR_BLOCKS) return pin->i_block[block_pos];

    /* single indirect */
    if (block_pos < doub_ind_s) {
        b = pin->i_block[EXT2_NDIR_BLOCKS]; /* address of single indirect block
                                             */
        index = block_pos - EXT2_NDIR_BLOCKS;
    } else if (block_pos >= out_range_s) {
        return NO_BLOCK;
    } else {
        /* double or triple indirect block. At first if it's triple,
         * find double indirect block.
         */
        excess = block_pos - doub_ind_s;
        b = pin->i_block[EXT2_DIND_BLOCK];
        if (block_pos >= triple_ind_s) {
            b = pin->i_block[EXT2_TIND_BLOCK];
            if (b == NO_BLOCK) return b;

            retval = fsdriver_get_block(&bp, pin->i_dev, b);
            if (retval) {
                err_code = retval;
                return NO_BLOCK;
            }

            excess = block_pos - triple_ind_s;
            index = excess / addr_in_block2;
            b = *(block_t*)(bp->data + sizeof(block_t) *
                                           index); /* num of double ind block */
            excess = excess % addr_in_block2;
            fsdriver_put_block(bp);
        }

        if (b == 0) return b;
        retval = fsdriver_get_block(&bp, pin->i_dev, b);
        index = excess / addr_in_block;
        b = *(block_t*)(bp->data +
                        sizeof(block_t) * index); /* num of single ind block */
        index = excess % addr_in_block; /* index into single ind blk */
        fsdriver_put_block(bp);
    }

    if (b == NO_BLOCK) return b;

    retval = fsdriver_get_block(&bp, pin->i_dev, b);
    if (retval) {
        err_code = retval;
        return NO_BLOCK;
    }

    b = *(block_t*)(bp->data + sizeof(block_t) * index);
    fsdriver_put_block(bp);

    return b;
}

int ext2_write_map(ext2_inode_t* pin, loff_t position, block_t new_block)
{
    int retval;
    block_t b1, b2, b3;
    int index1, index2, index3 = 0;
    int new_ind = 0, new_dbl = 0, new_triple = 0;
    int single = 0, triple = 0;
    unsigned long excess, block_pos;
    static int first_time = TRUE;
    static loff_t addr_in_block;
    static loff_t addr_in_block2;
    /* double indirect slots */
    static loff_t doub_ind_s;
    /* triple indirect slots */
    static loff_t triple_ind_s;
    static loff_t out_range_s;

    struct fsdriver_buffer *bp = NULL, *bp_dindir = NULL, *bp_tindir = NULL;

    if (first_time) {
        addr_in_block = pin->i_sb->sb_block_size / EXT2_BLOCK_ADDRESS_BYTES;
        addr_in_block2 = addr_in_block * addr_in_block;
        doub_ind_s = EXT2_NDIR_BLOCKS + addr_in_block;
        triple_ind_s = doub_ind_s + addr_in_block2;
        out_range_s = triple_ind_s + addr_in_block2 * addr_in_block;
        first_time = FALSE;
    }

    block_pos = position / pin->i_sb->sb_block_size;
    pin->i_dirt = INO_DIRTY; /* inode will be dirty anyway */

    if (block_pos < EXT2_NDIR_BLOCKS) {
        pin->i_block[block_pos] = new_block;
        pin->i_blocks += pin->i_sb->sb_block_size / 512;
        return 0;
    }

    if (block_pos < doub_ind_s) {
        b1 = pin->i_block[EXT2_NDIR_BLOCKS];
        index1 = block_pos - EXT2_NDIR_BLOCKS;
        single = TRUE;
    } else if (block_pos >= out_range_s)
        return EFBIG;
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
            } else {
                retval = fsdriver_get_block(&bp_tindir, pin->i_dev, b3);
                if (new_triple) {
                    fsdriver_mark_dirty(bp);
                }
                excess = block_pos - triple_ind_s;
                index3 = excess / addr_in_block2;
                b2 = *(block_t*)(bp_tindir->data + sizeof(block_t) * index3);
                excess = excess % addr_in_block2;
            }
            triple = TRUE;
        }

        if (b2 == 0) {
            if ((b2 = ext2_alloc_block(pin)) == 0) {
                fsdriver_put_block(bp_tindir);
                return (ENOSPC);
            }
            if (triple) {
                *(block_t*)(bp_tindir->data + sizeof(block_t) * index3) =
                    b2; /* update triple indir */
                fsdriver_mark_dirty(bp_tindir);
            } else {
                pin->i_block[EXT2_DIND_BLOCK] = b2;
            }
            pin->i_blocks += pin->i_sb->sb_block_size / 512;
            new_dbl = TRUE; /* set flag for later */
        }

        if (b2 != 0) {
            retval = fsdriver_get_block(&bp_dindir, pin->i_dev, b2);
            if (new_dbl) {
                fsdriver_mark_dirty(bp_dindir);
            }
            index2 = excess / addr_in_block;
            b1 = *(block_t*)(bp_dindir->data + sizeof(block_t) * index2);
            index1 = excess % addr_in_block;
        }
        single = FALSE;
    }

    if (b1 == NO_BLOCK) {
        if ((b1 = ext2_alloc_block(pin)) == 0) {
            fsdriver_put_block(bp_dindir);
            fsdriver_put_block(bp_tindir);
            return ENOSPC;
        }
        if (single) {
            pin->i_block[EXT2_NDIR_BLOCKS] =
                b1; /* update inode single indirect */
        } else {
            *(block_t*)(bp_dindir->data + sizeof(block_t) * index2) =
                b1; /* update dbl indir */
            fsdriver_mark_dirty(bp_dindir);
        }
        pin->i_blocks += pin->i_sb->sb_block_size / 512;
        new_ind = 1;
    }

    if (b1 != 0) {
        retval = fsdriver_get_block(&bp, pin->i_dev, b1);

        if (new_ind) {
            memset(bp->data, 0, bp->size);
        }

        *(block_t*)(bp->data + sizeof(block_t) * index1) = new_block;
        pin->i_blocks += pin->i_sb->sb_block_size / 512;

        if (b1 != 0) {
            fsdriver_mark_dirty(bp);
        }

        fsdriver_put_block(bp);
    }

    fsdriver_put_block(bp_dindir);
    fsdriver_put_block(bp_tindir);

    return 0;
}

struct fsdriver_buffer* ext2_new_block(ext2_inode_t* pin, off_t position)
{
    int retval;
    struct fsdriver_buffer* bp;
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

    retval = fsdriver_get_block(&bp, pin->i_dev, b);

    if (retval) {
        err_code = retval;
        return NULL;
    }

    return bp;
}

static int dirent_type(ext2_dir_entry_t* dp)
{
    switch (dp->d_file_type) {
    case EXT2_FT_REG_FILE:
        return DT_REG;
    case EXT2_FT_DIR:
        return DT_DIR;
    case EXT2_FT_SYMLINK:
        return DT_LNK;
    case EXT2_FT_BLKDEV:
        return DT_BLK;
    case EXT2_FT_CHRDEV:
        return DT_CHR;
    case EXT2_FT_FIFO:
        return DT_FIFO;
    default:
        return DT_UNKNOWN;
    }
}

ssize_t ext2_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                      loff_t* ppos, size_t count)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + EXT2_NAME_LEN + 1)
#define GETDENTS_ENTRIES 8
    static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];

    struct fsdriver_buffer* bp;
    ssize_t retval = 0;
    int done = 0;
    loff_t pos = *ppos;

    ext2_inode_t* pin = find_ext2_inode(dev, num);
    if (!pin) return -EINVAL;

    size_t block_size = pin->i_sb->sb_block_size;
    off_t block_off = pos % block_size; /* offset in block */
    off_t block_pos = pos - block_off;  /* block position */

    off_t newpos = pin->i_size;

    struct fsdriver_dentry_list list;
    fsdriver_dentry_list_init(&list, data, count, getdents_buf,
                              sizeof(getdents_buf));

    for (; block_pos < pin->i_size; block_pos += block_size) {
        block_t b = ext2_read_map(pin, block_pos);

        retval = fsdriver_get_block(&bp, pin->i_dev, b);
        if (retval) return -retval;

        ext2_dir_entry_t* dp = (ext2_dir_entry_t*)bp->data;

        off_t cur_pos = block_pos;

        for (; cur_pos + dp->d_rec_len <= pos &&
               (char*)EXT2_NEXT_DIR_ENTRY(dp) - bp->data < block_size;
             dp = EXT2_NEXT_DIR_ENTRY(dp)) {
            cur_pos += dp->d_rec_len;
        }

        for (; (char*)dp - (char*)bp->data < block_size;
             dp = EXT2_NEXT_DIR_ENTRY(dp)) {
            if (dp->d_inode == 0) continue;

            off_t ent_pos = block_pos + ((char*)dp - bp->data);

            retval = fsdriver_dentry_list_add(&list, dp->d_inode, dp->d_name,
                                              dp->d_name_len, dirent_type(dp));

            if (retval <= 0) {
                newpos = ent_pos;
                done = 1;
                break;
            }
        }

        fsdriver_put_block(bp);
        if (done) break;
    }

    if (retval >= 0 && (retval = fsdriver_dentry_list_finish(&list)) >= 0) {
        *ppos = newpos;
        pin->i_update |= ATIME;
        pin->i_dirt = 1;
    }

    put_ext2_inode(pin);

    return retval;
}
