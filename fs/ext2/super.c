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
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"

ext2_bgdescriptor_t * bgdescs = 0;

#define DEBUG
#ifdef DEBUG
#define DEB(x) printl("ext2fs: "); x
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
PUBLIC void read_ext2_super_block(int dev)
{
    int i;
    MESSAGE driver_msg;

    driver_msg.type     = DEV_READ;
    driver_msg.DEVICE   = MINOR(dev);
    // byte offset 1024
    driver_msg.POSITION = SECTOR_SIZE * 2;
    driver_msg.BUF      = fsbuf;
    // size 1024 bytes
    driver_msg.CNT      = SECTOR_SIZE * 2;
    driver_msg.PROC_NR  = TASK_FS;
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);

    /* find a free slot in super_block[] */
    for (i = 0; i < NR_SUPER_BLOCK; i++)
        if (super_block[i].sb_dev == NO_DEV)
            break;
    if (i == NR_SUPER_BLOCK)
        panic("super_block slots used up");

    ext2_superblock_t * pext2sb = (ext2_superblock_t *)fsbuf;
    DEB(printl("File system information:\n"));
    DEB(printl("  Magic: 0x%x\n", pext2sb->magic));
    assert(pext2sb->magic == EXT2FS_MAGIC);
    DEB(printl("  Inodes: %d\n", pext2sb->inodes_count));
    DEB(printl("  Blocks: %d\n", pext2sb->blocks_count));
    super_block[i].block_size = 1 << (pext2sb->log_block_size + 10);
    DEB(printl("  Block size: %d bytes\n", super_block[i].block_size));

    super_block[i].EXT2_SB = *pext2sb;
    super_block[i].sb_dev = dev;
}

PUBLIC void ext2_mount_fs(int dev)
{
    DEB(printl("Mounting ext2fs on dev = %d\n", dev));

    read_ext2_super_block(dev);

    struct super_block * psb = get_super_block(dev);
    int block_size = psb->block_size;

    int nr_groups = (psb->EXT2_SB.blocks_count - psb->EXT2_SB.first_data_block - 1) / psb->EXT2_SB.blocks_per_group + 1;
    int inodes_per_group = psb->EXT2_SB.inodes_count / nr_groups;
    DEB(printl("Total block groups: %d\n", nr_groups));
    DEB(printl("Inodes per groups: %d\n", inodes_per_group));
    
    int bgdesc_blocks = sizeof(ext2_bgdescriptor_t) * nr_groups / block_size + 1;
    int bgdesc_offset = 1024 / block_size + 1;

    bgdescs = (ext2_bgdescriptor_t*)alloc_mem(bgdesc_blocks * block_size);
    assert(bgdescs != (ext2_bgdescriptor_t*)0);
    DEB(printl("Allocated block group descriptors memory: 0x%x, size: %d bytes\n", bgdescs, bgdesc_blocks * block_size));

    read_ext2_blocks(dev, bgdesc_offset, bgdesc_blocks, getpid(), bgdescs);

#ifdef EXT2_BGDESCRIPTORS_DEBUG
    printl("Block group descriptors:\n");
    int i;
    for(i = 0; i < nr_groups; i++)
    {
        printl("  Block group #%d\n", i);
        printl("    Inode table: %d\n", bgdescs[i].inode_table);
        printl("    Free blocks: %d\n", bgdescs[i].free_blocks_count);
        printl("    Free inodes: %d\n", bgdescs[i].free_inodes_count);
    }
#endif
    
}