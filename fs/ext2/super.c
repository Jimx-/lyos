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
#include "ext2_fs.h"
#include "global.h"

#include <libbdev/libbdev.h>
#include <libdevman/libdevman.h>

//#define DEBUG
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
PUBLIC int read_ext2_super_block(dev_t dev)
{
    MESSAGE driver_msg;

    driver_msg.type = BDEV_READ;
    driver_msg.DEVICE = MINOR(dev);
    // byte offset 1024
    driver_msg.POSITION = 1024;
    driver_msg.BUF = ext2fsbuf;
    // size 1024 bytes
    driver_msg.CNT = EXT2_SUPERBLOCK_SIZE;
    driver_msg.PROC_NR = ext2_ep;
    endpoint_t driver_ep = dm_get_bdev_driver(dev);
    int retval;
    if ((retval = send_recv(BOTH, driver_ep, &driver_msg)) != 0) return retval;

    ext2_superblock_t* pext2sb =
        (ext2_superblock_t*)malloc(sizeof(ext2_superblock_t));
    if (!pext2sb) return ENOMEM;

    DEB(printl("Allocated super block at 0x%x\n", (unsigned int)pext2sb));
    memcpy((void*)pext2sb, ext2fsbuf, EXT2_SUPERBLOCK_SIZE);

    DEB(printl("File system information:\n"));
    DEB(printl("  Magic: 0x%x\n", pext2sb->sb_magic));
    if (pext2sb->sb_magic != EXT2FS_MAGIC) {
        free(pext2sb);
        return EINVAL;
    }
    DEB(printl("  Inodes: %d\n", pext2sb->sb_inodes_count));
    DEB(printl("  Blocks: %d\n", pext2sb->sb_blocks_count));
    pext2sb->sb_block_size = 1 << (pext2sb->sb_log_block_size + 10);
    DEB(printl("  Block size: %d bytes\n", pext2sb->sb_block_size));
    pext2sb->sb_groups_count =
        (pext2sb->sb_blocks_count - pext2sb->sb_first_data_block - 1) /
            pext2sb->sb_blocks_per_group +
        1;
    pext2sb->sb_blocksize_bits = pext2sb->sb_log_block_size + 10;
    pext2sb->sb_dev = dev;
    list_add(&(pext2sb->list), &ext2_superblock_table);

    int block_size = pext2sb->sb_block_size;

    int nr_groups = pext2sb->sb_groups_count;
    DEB(printl("Total block groups: %d\n", nr_groups));
    DEB(printl("Inodes per groups: %d\n",
               pext2sb->sb_inodes_count / nr_groups));
    int bgdesc_blocks =
        sizeof(ext2_bgdescriptor_t) * nr_groups / block_size + 1;
    int bgdesc_offset = 1024 / block_size + 1;

    pext2sb->sb_bgdescs =
        (ext2_bgdescriptor_t*)malloc(bgdesc_blocks * block_size);
    if (!(pext2sb->sb_bgdescs)) {
        free(pext2sb);
        return ENOMEM;
    }
    DEB(printl(
        "Allocated block group descriptors memory: 0x%x, size: %d bytes\n",
        pext2sb->sb_bgdescs, bgdesc_blocks * block_size));

    int i;
    ext2_buffer_t* pb = NULL;
    for (i = 0; i < bgdesc_blocks; i++) {
        if ((pb = ext2_get_buffer(dev, bgdesc_offset + i)) == NULL)
            return err_code;
        memcpy((void*)((int)pext2sb->sb_bgdescs + block_size * i), pb->b_data,
               block_size);
        ext2_put_buffer(pb);
    }

//#define EXT2_BGDESCRIPTORS_DEBUG
#ifdef EXT2_BGDESCRIPTORS_DEBUG
    printl("Block group descriptors:\n");
    for (i = 0; i < nr_groups; i++) {
        printl("  Block group #%d\n", i);
        printl("    Inode table: %d\n", pext2sb->sb_bgdescs[i].inode_table);
        printl("    Free blocks: %d\n",
               pext2sb->sb_bgdescs[i].free_blocks_count);
        printl("    Free inodes: %d\n",
               pext2sb->sb_bgdescs[i].free_inodes_count);
    }
#endif

    return 0;
}

PUBLIC int write_ext2_super_block(dev_t dev)
{
    ext2_superblock_t* psb = get_ext2_super_block(dev);
    if (!psb) return EINVAL;

    MESSAGE driver_msg;
    driver_msg.type = BDEV_WRITE;
    driver_msg.DEVICE = MINOR(dev);
    // byte offset 1024
    driver_msg.POSITION = 1024;
    driver_msg.BUF = psb;
    // size 1024 bytes
    driver_msg.CNT = EXT2_SUPERBLOCK_SIZE;
    driver_msg.PROC_NR = ext2_ep;
    endpoint_t driver_ep = dm_get_bdev_driver(dev);
    send_recv(BOTH, driver_ep, &driver_msg);

    return 0;
}

PUBLIC ext2_superblock_t* get_ext2_super_block(dev_t dev)
{
    ext2_superblock_t* psb;
    list_for_each_entry(psb, &ext2_superblock_table, list)
    {
        if (psb->sb_dev == dev) return psb;
    }
    printl("ext2fs: Cannot find super block on dev = %d\n", dev);
    return NULL;
}

PUBLIC ext2_bgdescriptor_t* get_ext2_group_desc(ext2_superblock_t* psb,
                                                unsigned int desc_num)
{
    if (desc_num >= psb->sb_groups_count) {
        printl("ext2fs: get_group_desc: wrong group descriptor number (%d) "
               "requested.\n",
               desc_num);
        return NULL;
    }
    return &(psb->sb_bgdescs[desc_num]);
}

/**
 * @brief ext2_readsuper Handle the FS_READSUPER request
 *
 * @param p Ptr to request message
 *
 * @return Zero if successful
 */
PUBLIC int ext2_readsuper(dev_t dev, int flags, struct fsdriver_node* node)
{
    int readonly = (flags & RF_READONLY) ? 1 : 0;
    int is_root = (flags & RF_ISROOT) ? 1 : 0;

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

    if ((pin->i_mode & I_TYPE) != I_DIRECTORY) {
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

PUBLIC int ext2_update_group_desc(ext2_superblock_t* psb, int desc)
{
    int block_size = psb->sb_block_size;

    int bgdesc_offset = 1024 / block_size + 1;
    int i = (&psb->sb_bgdescs[desc] - &psb->sb_bgdescs[0]) / block_size;
    bgdesc_offset += i;

    ext2_buffer_t* pb = NULL;
    if ((pb = ext2_get_buffer(psb->sb_dev, bgdesc_offset)) == NULL)
        return err_code;

    memcpy(pb->b_data, (void*)((int)psb->sb_bgdescs + block_size * i),
           block_size);
    pb->b_dirt = 1;
    pb->b_flags |= EXT2_BUFFER_WRITE_IMME;
    ext2_put_buffer(pb);

    return 0;
}
