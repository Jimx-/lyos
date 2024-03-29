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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/sysutils.h>
#include "errno.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"

//#define INODE_DEBUG
#ifdef INODE_DEBUG
#define DEB(x)          \
    printl("ext2fs: "); \
    x
#else
#define DEB(x)
#endif

static ext2_inode_t* ext2_alloc_unused_inode();
static void ext2_release_inode(ext2_inode_t* pin);
static int rw_inode(ext2_inode_t* inode, int rw_flag);

void ext2_init_inode()
{
    int i;
    for (i = 0; i < EXT2_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&ext2_inode_table[i]);
    }

    INIT_LIST_HEAD(&ext2_unused_inode_list);
    for (i = 0; i < EXT2_NR_INODES; i++) {
        list_add(&(ext2_inode_buffer[i].list), &ext2_unused_inode_list);
    }
}

static ext2_inode_t* ext2_alloc_unused_inode()
{
    if (list_empty(&ext2_unused_inode_list)) panic("ext2: inode table full.");

    ext2_inode_t* ret =
        list_entry((&ext2_unused_inode_list)->next, ext2_inode_t, list);
    list_del(&(ret->list));

    return ret;
}

static void ext2_release_inode(ext2_inode_t* pin)
{
    list_add(&(pin->list), &ext2_unused_inode_list);
}

static void ext2_addhash_inode(ext2_inode_t* inode)
{
    unsigned int hash = inode->i_num & EXT2_INODE_HASH_MASK;

    list_add(&(inode->list), &ext2_inode_table[hash]);
}

static void ext2_unhash_inode(ext2_inode_t* inode) { list_del(&(inode->list)); }

ext2_inode_t* get_ext2_inode(dev_t dev, ino_t num)
{
    int hash = num & EXT2_INODE_HASH_MASK;

    ext2_inode_t* inode;
    /* first look for it in the hash table */
    list_for_each_entry(inode, &ext2_inode_table[hash], list)
    {
        if ((inode->i_num == num) && (inode->i_dev == dev)) { /* hit */
            ++inode->i_count;
            return inode;
        }
    }

    /* not found. allocate one */
    inode = ext2_alloc_unused_inode();
    assert(inode != NULL);
    DEB(printl("Allocated inode at 0x%x\n", (unsigned int)inode));

    /* fill the inode */
    inode->i_dev = dev;
    inode->i_num = num;
    inode->i_count = 1;
    rw_inode(inode, READ); /* get it from the disk */

    /* add it to hash table */
    ext2_addhash_inode(inode);
    inode->i_dirt = INO_CLEAN;
    return inode;
}

ext2_inode_t* find_ext2_inode(dev_t dev, ino_t num)
{
    int hash = num & EXT2_INODE_HASH_MASK;

    ext2_inode_t* inode;
    list_for_each_entry(inode, &ext2_inode_table[hash], list)
    {
        if ((inode->i_num == num) && (inode->i_dev == dev)) { /* hit */
            return inode;
        }
    }
    return NULL;
}

void put_ext2_inode(ext2_inode_t* pin)
{
    if (!pin) return;

    assert(pin->i_count > 0);

    --pin->i_count;

    /* no one is using it */
    if (!pin->i_count) {
        if (pin->i_dirt == INO_DIRTY) rw_inode(pin, WRITE);
        ext2_unhash_inode(pin);
        ext2_release_inode(pin);
    }
}

int ext2_putinode(dev_t dev, ino_t num, unsigned int count)
{
    ext2_inode_t* pin = find_ext2_inode(dev, num);

    assert(pin);
    assert(count <= pin->i_count);

    pin->i_count -= count - 1;

    put_ext2_inode(pin);

    return 0;
}

static void update_times(ext2_inode_t* pin)
{
    if (pin->i_update == 0) return;

    u32 timestamp = now();

    if (pin->i_update & CTIME) pin->i_ctime = timestamp;
    if (pin->i_update & MTIME) pin->i_mtime = timestamp;
    if (pin->i_update & ATIME) pin->i_atime = timestamp;
    pin->i_update = 0;
}

int ext2_rw_inode(ext2_inode_t* inode, int rw_flag)
{
    return rw_inode(inode, rw_flag);
}

static int rw_inode(ext2_inode_t* inode, int rw_flag)
{
    dev_t dev = inode->i_dev;
    struct fsdriver_buffer* bp;
    int retval;

    ext2_superblock_t* psb = get_ext2_super_block(dev);
    inode->i_sb = psb;
    int desc_num = (inode->i_num - 1) / psb->sb_inodes_per_group;
    ext2_bgdescriptor_t* bgdesc = get_ext2_group_desc(psb, desc_num);

    if (bgdesc == NULL)
        panic("ext2fs: Can't get block descriptor to read/write inode");

    int offset =
        ((inode->i_num - 1) % psb->sb_inodes_per_group) * EXT2_INODE_SIZE(psb);
    block_t block_nr =
        (block_t)bgdesc->inode_table + (offset >> psb->sb_blocksize_bits);

    /* read the inode table */
    offset &= (psb->sb_block_size - 1);
    retval = fsdriver_get_block(&bp, dev, block_nr);
    if (retval) return retval;

    if (rw_flag == READ) {
        memcpy(inode, bp->data + offset, EXT2_GOOD_OLD_INODE_SIZE);
    } else if (rw_flag == WRITE) {
        if (inode->i_update) update_times(inode);
        memcpy(bp->data + offset, inode, EXT2_GOOD_OLD_INODE_SIZE);
        fsdriver_mark_dirty(bp);
        /* write back to the device */
    } else
        return EINVAL;

    fsdriver_put_block(bp);

    inode->i_dirt = INO_CLEAN;

    return 0;
}

/**
 * <Ring 1> Print pin's information.
 * @param pin The inode.
 */
void ext2_dump_inode(ext2_inode_t* pin)
{
    printl("-------------------------\n");
    printl("Ext2 Inode #%d @ dev %x\n", pin->i_num, pin->i_dev);
    printl("  Mode: 0x%x, Size: %d\n", pin->i_mode, pin->i_size);
    printl("  atime: %lu\n  ctime: %lu\n  mtime: %lu\n", pin->i_atime,
           pin->i_ctime, pin->i_mtime);
    printl("  User: %d, Group: %d\n", pin->i_uid, pin->i_gid);
    printl("  Blocks: %d, Links: %d\n", pin->i_blocks, pin->i_links_count);
    printl("-------------------------\n");
}

/**
 * <Ring 1> Synchronize all inodes.
 */
void ext2_sync_inodes()
{
    int i;
    for (i = 0; i < EXT2_INODE_HASH_SIZE; i++) {
        ext2_inode_t* pin;
        list_for_each_entry(pin, &ext2_inode_table[i], list)
        {
            if (pin->i_dirt) {
                DEB(printl("Writing inode #%d at dev 0x%x\n", pin->i_num,
                           pin->i_dev));
                ext2_rw_inode(pin, WRITE);
            }
        }
    }
}
