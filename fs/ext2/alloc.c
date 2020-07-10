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
#include "stdio.h"
#include "unistd.h"
#include "stddef.h" /* for NULL */
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "errno.h"
#include "fcntl.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

/* how many bits in a char */
#define CHAR_BIT 8

static int ext2_alloc_inode_bit(ext2_superblock_t* psb, ext2_inode_t* parent,
                                mode_t mode);
static void wipe_inode(ext2_inode_t* pin);

int ext2_setbit(bitchunk_t* bitmap, int max_bits, off_t startp)
{
    int b = -1;
    bitchunk_t *chunk, *limit = &bitmap[(max_bits >> 3) / sizeof(bitchunk_t)];

    for (chunk = &bitmap[startp]; chunk < limit; chunk++) {
        int i;
        bitchunk_t k;

        /* no free bits */
        if (*chunk == (bitchunk_t)~0) continue;

        /* find and allocate a free bit */
        k = (bitchunk_t)*chunk;
        for (i = 0; (k & (1 << i)) != 0; ++i) {
        }

        /* bit number */
        b = (chunk - &bitmap[0]) * sizeof(bitchunk_t) * CHAR_BIT + i;

        /* beyond limit */
        if (b >= max_bits) {
            b = -1;
            continue;
        }

        /* allocate bit */
        k |= 1 << i;
        *chunk = (bitchunk_t)k;
        break;
    }

    return b;
}

block_t ext2_alloc_block(ext2_inode_t* pin)
{
    int retval;
    ext2_superblock_t* psb = pin->i_sb;
    block_t block = 0;

    if (psb->sb_readonly) {
        printl("ext2fs: alloc_block: try to allocate block on readonly "
               "filesystem!");
        err_code = EROFS;
        return 0;
    }

    int group = (pin->i_num - 1) / psb->sb_inodes_per_group;
    block_t goal = psb->sb_blocks_per_group * group + psb->sb_first_data_block;
    off_t startp =
        ((goal - psb->sb_first_data_block) % psb->sb_blocks_per_group) /
        (sizeof(bitchunk_t) * CHAR_BIT);

    int i;
    for (i = 0; i <= psb->sb_groups_count; i++, group++) {
        struct fsdriver_buffer* bp;
        ext2_bgdescriptor_t* gd;

        if (group >= psb->sb_groups_count) group = 0;

        gd = get_ext2_group_desc(psb, group);
        if (gd == NULL)
            panic("ext2fs: alloc_block: can't get block group descriptor to "
                  "alloc block");

        if (gd->free_blocks_count == 0) {
            startp = 0;
            continue;
        }

        retval = fsdriver_get_block(&bp, psb->sb_dev, gd->block_bitmap);
        if (retval) continue;

        int bit = ext2_setbit((bitchunk_t*)bp->data, psb->sb_blocks_per_group,
                              startp);

        if (bit == -1) {
            if (startp == 0) {
                panic("ext2fs: alloc_block: failed to allocate bit in bitmap "
                      "with free bits");
            } else {
                startp = 0;
                continue;
            }
        }

        block =
            psb->sb_first_data_block + group * psb->sb_blocks_per_group + bit;

        fsdriver_mark_dirty(bp);
        fsdriver_put_block(bp);

        gd->free_blocks_count--;
        psb->sb_free_blocks_count--;
        ext2_update_group_desc(psb, group);

        return block;
    }

    if (retval) {
        err_code = retval;
    }

    return block;
}

/**
 * <Ring 1> Allocate an inode in parent.
 * @param  parent The parent directory.
 * @param  mode   Inode mode.
 * @return        The inode allocated.
 */
ext2_inode_t* ext2_alloc_inode(ext2_inode_t* parent, mode_t mode, uid_t uid,
                               gid_t gid)
{
    ext2_superblock_t* psb = parent->i_sb;
    /* Can't allocate inode on readonly filesystem */
    if (psb->sb_readonly) {
        err_code = EROFS;
        return NULL;
    }

    ino_t num = ext2_alloc_inode_bit(psb, parent, mode);

    if (num == 0) {
        err_code = ENOSPC;
        return NULL;
    }

    ext2_inode_t* pin = get_ext2_inode(psb->sb_dev, num);

    if (pin) {
        pin->i_mode = mode;
        pin->i_links_count = 0;
        pin->i_uid = uid;
        pin->i_gid = gid;
        pin->i_dev = parent->i_dev;
        pin->i_sb = psb;

        wipe_inode(pin);
    }

    return pin;
}

/**
 * Allocate a bit in the inode bitmap.
 * @param  psb    Ptr to the superblock.
 * @param  parent The parent directory.
 * @param  mode   Inode mode.
 * @return        Inode number.
 */
static int ext2_alloc_inode_bit(ext2_superblock_t* psb, ext2_inode_t* parent,
                                mode_t mode)
{
    int retval;

    /* Can't allocate inode on readonly filesystem */
    if (psb->sb_readonly) {
        panic("ext2fs: ext2_alloc_inode_bit: can't allocate inode on readonly "
              "filesystem.");
    }

    ext2_bgdescriptor_t* group = NULL;
    ext2_bgdescriptor_t* bgdesc = psb->sb_bgdescs;
    int i;
    for (i = 0; i < psb->sb_groups_count; i++, bgdesc++) {
        if (bgdesc->free_inodes_count > 0) {
            group = bgdesc;
            break;
        }
    }

    /* No space */
    if (group == NULL) return 0;

    struct fsdriver_buffer* bp;
    retval = fsdriver_get_block(&bp, psb->sb_dev, group->inode_bitmap);
    if (retval) {
        err_code = retval;
        return 0;
    }

    int bit = ext2_setbit((bitchunk_t*)bp->data, psb->sb_inodes_per_group, 0);
    ino_t num = (group - psb->sb_bgdescs) * psb->sb_inodes_per_group + bit + 1;

    if (num > psb->sb_inodes_count) {
        panic("ext2fs: ext2_alloc_inode_bit: num > total number of inodes.");
    }
    if (num < psb->sb_first_ino) {
        panic("ext2fs: ext2_alloc_inode_bit: try to allocate reserved inode.");
    }

    fsdriver_mark_dirty(bp);
    fsdriver_put_block(bp);

    psb->sb_free_inodes_count--;
    group->free_inodes_count--;

    /* write back the changes */
    write_ext2_super_block(psb->sb_dev);
    ext2_update_group_desc(psb, group - psb->sb_bgdescs);

    return num;
}

static void wipe_inode(ext2_inode_t* pin)
{
    int i;

    pin->i_size = 0;
    pin->i_update = ATIME | CTIME | MTIME;
    pin->i_blocks = 0;
    pin->i_flags = 0;
    pin->i_generation = 0;
    pin->i_file_acl = 0;
    pin->i_dir_acl = 0;
    pin->i_faddr = 0;

    for (i = 0; i < EXT2_N_BLOCKS; i++) {
        pin->i_block[i] = NO_BLOCK;
    }

    pin->i_dirt = INO_DIRTY;
}
