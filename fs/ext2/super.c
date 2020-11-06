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
#include "errno.h"
#include "stdio.h"
#include <stdlib.h>
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include "ext2_fs.h"
#include "global.h"

#include <libdevman/libdevman.h>
#include <libbdev/libbdev.h>

/* #define DEBUG */
#if defined(DEBUG)
#define DEB(x)          \
    printl("ext2fs: "); \
    x
#else
#define DEB(x)
#endif

/*****************************************************************************
 *                                read_super_block
 *****************************************************************************/
/**
 * <Ring 1> Read super block from the given device then write it into a free
 *          super_block[] slot.
 *
 * @param dev  From which device the super block comes.
 *****************************************************************************/
int read_ext2_super_block(dev_t dev)
{
    ssize_t retval;

    retval = bdev_read(dev, 1024, ext2fsbuf, EXT2_SUPERBLOCK_SIZE);
    if (retval < 0) return -retval;

    ext2_superblock_t* psb =
        mmap(NULL, sizeof(ext2_superblock_t), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (psb == MAP_FAILED) return ENOMEM;

    DEB(printl("Allocated super block at 0x%x\n", (unsigned int)psb));
    memcpy((void*)psb, ext2fsbuf, EXT2_SUPERBLOCK_SIZE);

    DEB(printl("File system information:\n"));
    DEB(printl("  Magic: 0x%x\n", psb->sb_magic));

    if (psb->sb_magic != EXT2FS_MAGIC) {
        munmap(psb, sizeof(ext2_superblock_t));
        return EINVAL;
    }

    DEB(printl("  Inodes: %d\n", psb->sb_inodes_count));
    DEB(printl("  Blocks: %d\n", psb->sb_blocks_count));
    psb->sb_block_size = 1 << (psb->sb_log_block_size + 10);
    DEB(printl("  Inode size: %d bytes\n", EXT2_INODE_SIZE(psb)));
    DEB(printl("  Block size: %d bytes\n", psb->sb_block_size));
    DEB(printl("  First Inode: %d\n", EXT2_FIRST_INO(psb)));
    DEB(printl("  Rev. level: %d.%d\n", psb->sb_rev_level,
               psb->sb_minor_rev_level));

    psb->sb_groups_count =
        (psb->sb_blocks_count - psb->sb_first_data_block - 1) /
            psb->sb_blocks_per_group +
        1;
    psb->sb_blocksize_bits = psb->sb_log_block_size + 10;
    psb->sb_inodes_per_block = psb->sb_block_size / EXT2_INODE_SIZE(psb);
    psb->sb_desc_per_block = psb->sb_block_size / sizeof(ext2_bgdescriptor_t);
    psb->sb_dev = dev;
    list_add(&(psb->list), &ext2_superblock_table);

    size_t block_size = psb->sb_block_size;

    int nr_groups = psb->sb_groups_count;
    DEB(printl("Total block groups: %d\n", nr_groups));
    DEB(printl("Inodes per groups: %d\n", psb->sb_inodes_count / nr_groups));
    int bgdesc_blocks =
        ((nr_groups + psb->sb_desc_per_block - 1) / psb->sb_desc_per_block);
    int bgdesc_offset = 1024 / block_size + 1;

    psb->sb_bgdescs =
        mmap(NULL, bgdesc_blocks * block_size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (psb->sb_bgdescs == MAP_FAILED) {
        munmap(psb, sizeof(ext2_superblock_t));
        return ENOMEM;
    }

    DEB(printl(
        "Allocated block group descriptors memory: 0x%x, size: %d bytes\n",
        psb->sb_bgdescs,
        bgdesc_blocks * block_size * sizeof(ext2_bgdescriptor_t)));

    int i;
    struct fsdriver_buffer* bp;
    for (i = 0; i < bgdesc_blocks; i++) {
        if ((retval = fsdriver_get_block(&bp, dev, bgdesc_offset + i)) != 0)
            return retval;
        memcpy((char*)psb->sb_bgdescs + block_size * i, bp->data, block_size);
        fsdriver_put_block(bp);
    }

/* #define EXT2_BGDESCRIPTORS_DEBUG */
#ifdef EXT2_BGDESCRIPTORS_DEBUG
    printl("Block group descriptors:\n");
    for (i = 0; i < nr_groups; i++) {
        printl("  Block group #%d\n", i);
        printl("    Inode table: %d\n", psb->sb_bgdescs[i].inode_table);
        printl("    Free blocks: %d\n", psb->sb_bgdescs[i].free_blocks_count);
        printl("    Free inodes: %d\n", psb->sb_bgdescs[i].free_inodes_count);
    }
#endif

    psb->sb_bsearch = psb->sb_first_data_block + 1 + psb->sb_groups_count + 2 +
                      psb->sb_inodes_per_group / psb->sb_inodes_per_block;

    return 0;
}

int write_ext2_super_block(dev_t dev)
{
    ssize_t retval;
    ext2_superblock_t* psb = get_ext2_super_block(dev);
    if (!psb) return EINVAL;

    retval = bdev_write(dev, 1024, psb, EXT2_SUPERBLOCK_SIZE);
    if (retval < 0) return -retval;

    return 0;
}

ext2_superblock_t* get_ext2_super_block(dev_t dev)
{
    ext2_superblock_t* psb;
    list_for_each_entry(psb, &ext2_superblock_table, list)
    {
        if (psb->sb_dev == dev) return psb;
    }
    printl("ext2fs: Cannot find super block on dev = %d\n", dev);
    return NULL;
}

ext2_bgdescriptor_t* get_ext2_group_desc(ext2_superblock_t* psb,
                                         unsigned int desc_num)
{
    if (desc_num >= psb->sb_groups_count) {
        printl("ext2fs: get_group_desc: wrong group descriptor number (%d) "
               "requested.\n",
               desc_num);
        return NULL;
    }

    return &psb->sb_bgdescs[desc_num];
}

/**
 * @brief ext2_readsuper Handle the FS_READSUPER request
 *
 * @param p Ptr to request message
 *
 * @return Zero if successful
 */
int ext2_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                   struct fsdriver_node* node)
{
    int readonly = !!(fc->sb_flags & RF_READONLY);
    int is_root = !!(fc->sb_flags & RF_ISROOT);

    /* open the device where this superblock resides on */
    bdev_open(dev);

    int retval = read_ext2_super_block(dev);
    if (retval) {
        bdev_close(dev);
        return retval;
    }

    ext2_superblock_t* psb = get_ext2_super_block(dev);

    ext2_inode_t* pin = get_ext2_inode(dev, EXT2_ROOT_INODE);
    if (!pin) {
        printl("ext2fs: ext2_readsuper(): cannot get root inode\n");
        bdev_close(dev);
        return EINVAL;
    }

    if (!S_ISDIR(pin->i_mode)) {
        printl("ext2fs: ext2_readsuper(): root inode is not a directory\n");
        bdev_close(dev);
        return EINVAL;
    }

    psb->sb_readonly = readonly;
    psb->sb_is_root = is_root;

    if (!readonly) {
        psb->sb_state = EXT2_ERROR_FS;
        psb->sb_mnt_count++;
        /* TODO: update mtime */
        write_ext2_super_block(dev);
    }

    /* fill result */
    node->fn_num = pin->i_num;
    node->fn_uid = pin->i_uid;
    node->fn_gid = pin->i_gid;
    node->fn_size = pin->i_size;
    node->fn_mode = pin->i_mode;

    return 0;
}

int ext2_update_group_desc(ext2_superblock_t* psb, int desc)
{
    int retval;
    struct fsdriver_buffer* bp;
    int block_size = psb->sb_block_size;
    int bgdesc_offset = 1024 / block_size + 1;
    int i = (&psb->sb_bgdescs[desc] - &psb->sb_bgdescs[0]) / block_size;

    bgdesc_offset += i;

    if ((retval = fsdriver_get_block(&bp, psb->sb_dev, bgdesc_offset)) != 0)
        return retval;

    memcpy(bp->data, (char*)psb->sb_bgdescs + block_size * i, block_size);

    fsdriver_mark_dirty(bp);
    fsdriver_put_block(bp);

    return 0;
}
