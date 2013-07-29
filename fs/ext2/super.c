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
#include "errno.h"
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
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

#define DEBUG
#if defined(DEBUG)
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
PUBLIC int read_ext2_super_block(int dev)
{
    MESSAGE driver_msg;

    driver_msg.type     = DEV_READ;
    driver_msg.DEVICE   = MINOR(dev);
    // byte offset 1024
    driver_msg.POSITION = SECTOR_SIZE * 2;
    driver_msg.BUF      = ext2fsbuf;
    // size 1024 bytes
    driver_msg.CNT      = SECTOR_SIZE * 2;
    driver_msg.PROC_NR  = getpid();
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);

    ext2_superblock_t * pext2sb = (ext2_superblock_t *)alloc_mem(sizeof(ext2_superblock_t));
    if (!pext2sb) return ENOMEM;
    DEB(printl("Allocated super block at 0x%x\n", (unsigned int)pext2sb));
    memcpy((void*)pext2sb,
                ext2fsbuf,
                EXT2_SUPERBLOCK_SIZE);

    DEB(printl("File system information:\n"));
    DEB(printl("  Magic: 0x%x\n", pext2sb->sb_magic));
    if (pext2sb->sb_magic != EXT2FS_MAGIC) {
        free_mem((int)pext2sb, sizeof(ext2_superblock_t));
        return EINVAL;
    }
    DEB(printl("  Inodes: %d\n", pext2sb->sb_inodes_count));
    DEB(printl("  Blocks: %d\n", pext2sb->sb_blocks_count));
    pext2sb->sb_block_size = 1 << (pext2sb->sb_log_block_size + 10);
    DEB(printl("  Block size: %d bytes\n", pext2sb->sb_block_size));
    pext2sb->sb_groups_count = (pext2sb->sb_blocks_count - pext2sb->sb_first_data_block - 1) / pext2sb->sb_blocks_per_group + 1;
    pext2sb->sb_blocksize_bits = pext2sb->sb_log_block_size + 10;
    pext2sb->sb_dev = dev;
    list_add(&(pext2sb->list), &ext2_superblock_table);
    
    int block_size = pext2sb->sb_block_size;

    int nr_groups = pext2sb->sb_groups_count;
    int inodes_per_group = pext2sb->sb_inodes_count / nr_groups;
    DEB(printl("Total block groups: %d\n", nr_groups));
    DEB(printl("Inodes per groups: %d\n", inodes_per_group));
    int bgdesc_blocks = sizeof(ext2_bgdescriptor_t) * nr_groups / block_size + 1;
    int bgdesc_offset = 1024 / block_size + 1;

    pext2sb->sb_bgdescs = (ext2_bgdescriptor_t*)alloc_mem(bgdesc_blocks * block_size);
    if (!(pext2sb->sb_bgdescs)) {
        free_mem((int)pext2sb, sizeof(ext2_superblock_t));
        return ENOMEM;
    }
    DEB(printl("Allocated block group descriptors memory: 0x%x, size: %d bytes\n", pext2sb->sb_bgdescs, bgdesc_blocks * block_size));

    rw_ext2_blocks(DEV_READ, dev, bgdesc_offset, bgdesc_blocks, pext2sb->sb_bgdescs);

#ifdef EXT2_BGDESCRIPTORS_DEBUG
    printl("Block group descriptors:\n");
    int i;
    for(i = 0; i < nr_groups; i++)
    {
        printl("  Block group #%d\n", i);
        printl("    Inode table: %d\n", pext2sb->sb_bgdescs[i].inode_table);
        printl("    Free blocks: %d\n", pext2sb->sb_bgdescs[i].free_blocks_count);
        printl("    Free inodes: %d\n", pext2sb->sb_bgdescs[i].free_inodes_count);
    }
#endif

    return 0;
}

PUBLIC int write_ext2_super_block(int dev)
{
    ext2_superblock_t * psb = get_ext2_super_block(dev);
    if (!psb) return EINVAL;

    memcpy(ext2fsbuf, psb, SECTOR_SIZE * 2);

    MESSAGE driver_msg;
    driver_msg.type     = DEV_WRITE;
    driver_msg.DEVICE   = MINOR(dev);
    // byte offset 1024
    driver_msg.POSITION = SECTOR_SIZE * 2;
    driver_msg.BUF      = ext2fsbuf;
    // size 1024 bytes
    driver_msg.CNT      = SECTOR_SIZE * 2;
    driver_msg.PROC_NR  = getpid();
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);

    return 0;
}

PUBLIC ext2_superblock_t * get_ext2_super_block(int dev)
{
    ext2_superblock_t * psb;
    list_for_each_entry(psb, &ext2_superblock_table, list) {
        if (psb->sb_dev == dev) return psb;
    }
    printl("ext2fs: Cannot find super block on dev = %d\n", dev);
    return (ext2_superblock_t *)0;
}

PUBLIC ext2_bgdescriptor_t * get_ext2_group_desc(ext2_superblock_t * psb, unsigned int desc_num)
{
    if (desc_num >= psb->sb_groups_count) {
        printl("ext2fs: get_group_desc: wrong group descriptor number (%d) requested.\n", desc_num);
        return NULL;
    }
    return &(psb->sb_bgdescs[desc_num]);
}


PRIVATE void bdev_open(dev_t dev)
{
    MESSAGE driver_msg;
	driver_msg.type = DEV_OPEN;
	driver_msg.DEVICE = MINOR(dev);
	assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
}

PRIVATE void bdev_close(dev_t dev)
{
    MESSAGE driver_msg;
	driver_msg.type = DEV_CLOSE;
	driver_msg.DEVICE = MINOR(dev);
	assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
	send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
}

/**
 * @brief ext2_readsuper Handle the FS_READSUPER request
 *
 * @param p Ptr to request message
 *
 * @return Zero if successful
 */
PUBLIC int ext2_readsuper(MESSAGE * p)
{
    dev_t dev = p->REQ_DEV3;

    int readonly = (p->REQ_FLAGS & RF_READONLY) ? 1 : 0;
    int is_root = (p->REQ_FLAGS & RF_ISROOT) ? 1 :0;

    /* open the device where this superblock resides on */
    bdev_open(dev);

    int retval = read_ext2_super_block(dev);
    if (retval) {
        bdev_close(dev);
        return retval;
    }

    ext2_superblock_t * psb = get_ext2_super_block(dev);

    ext2_inode_t * pin = get_ext2_inode(dev, EXT2_ROOT_INODE);
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
	p->RET_NUM = pin->i_num;
	p->RET_UID = pin->i_uid;
	p->RET_GID = pin->i_gid;
	p->RET_FILESIZE = pin->i_size;
	p->RET_MODE = pin->i_mode;
    
    return 0;
}

