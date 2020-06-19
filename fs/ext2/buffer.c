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
#include <stdlib.h>
#include "stddef.h"
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/list.h"
#include "ext2_fs.h"
#include "global.h"
#include "buffer.h"

#include <libbdev/libbdev.h>

//#define EXT2_BUFFER_DEBUG

static void ext2_rw_buffer(int rw_flag, ext2_buffer_t* pb);
static void rw_ext2_blocks(int rw_flag, int dev, int block_nr, int block_count,
                           void* buf);

static int nr_buffers;

void ext2_init_buffer_cache()
{
    int i;
    nr_buffers = 0;
    for (i = 0; i < EXT2_BUFFER_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&ext2_buffer_cache[i]);
    }
}

/**
 * @brief get_buffer Check if the required block is in the block cache, if it
 * is, return it.
 *
 * @param dev   Required device.
 * @param block Required block.
 *
 * @return Ptr to block buffer if found.
 */
ext2_buffer_t* ext2_get_buffer(dev_t dev, block_t block)
{
#define REPORT_ERROR_AND_RETURN(e)                                             \
    do {                                                                       \
        printl("ext2: warning: buffer operation failed(d%d, b%d, e%d)\n", dev, \
               block, e);                                                      \
        err_code = e;                                                          \
        return NULL;                                                           \
    } while (0)

    if (!dev) REPORT_ERROR_AND_RETURN(EINVAL);

    block_t hash = block % EXT2_BUFFER_HASH_SIZE;
    ext2_buffer_t* buf;
    list_for_each_entry(buf, &ext2_buffer_cache[hash], hash)
    {
        if (buf->b_dev == dev && buf->b_block == block) {
            buf->b_refcnt++;
            return buf;
        }
    }

    list_for_each_entry(buf, &ext2_buffer_freelist, list)
    {
        if (buf->b_dev == dev && buf->b_block == block) {
            buf->b_refcnt++;
            /* put it back to hash table */
            list_del(&(buf->list));
            list_add(&(buf->hash), &ext2_buffer_cache[hash]);
            return buf;
        }
    }

    ext2_superblock_t* psb = get_ext2_super_block(dev);
    if (!psb) REPORT_ERROR_AND_RETURN(EINVAL);

    size_t block_size = psb->sb_block_size;

    ext2_buffer_t* pb = NULL;

    if (nr_buffers < MAX_BUFFERS ||
        list_empty(&ext2_buffer_freelist)) { /* just allocate one */
        pb = (ext2_buffer_t*)malloc(sizeof(ext2_buffer_t));
        if (!pb) REPORT_ERROR_AND_RETURN(ENOMEM);

        pb->b_dev = dev;
        pb->b_block = block;
        pb->b_dirt = 0;
        pb->b_refcnt = 1;
        pb->b_size = block_size;

        /* allocate data area */
        pb->b_data = (char*)malloc(block_size);
        if (!pb->b_data) REPORT_ERROR_AND_RETURN(ENOMEM);
        nr_buffers++;
    } else { /* find a buffer in the freelist */
        pb = (ext2_buffer_t*)(ext2_buffer_freelist
                                  .next); /* pick the first one */
        if (pb->b_dirt)
            ext2_rw_buffer(WRITE, pb); /* write back to disk to make it clear */
        if (pb->b_size != block_size) { /* realloc */
            free(pb->b_data);
            pb->b_size = block_size;
            pb->b_data = (char*)malloc(pb->b_size);
            if (!pb->b_data) REPORT_ERROR_AND_RETURN(ENOMEM);
        }
        ext2_zero_buffer(pb);
        pb->b_dev = dev;
        pb->b_block = block;
        pb->b_refcnt = 1;
        list_del(&(pb->list));
    }

    pb->b_flags = 0;

    ext2_rw_buffer(READ, pb);
    list_add(&(pb->hash), &ext2_buffer_cache[hash]);
    return pb;
}

void ext2_put_buffer(ext2_buffer_t* pb)
{
    if (!pb) return;

    if (pb->b_refcnt < 1) {
        printl("ext2fs: ext2_put_buffer: try to free freed buffer\n");
        list_del(&(pb->hash));
        free(pb);
        return;
    }

    if (--pb->b_refcnt == 0) {
        /* put it to freelist */
        list_del(&(pb->hash));
        list_add(&(pb->list), ext2_buffer_freelist_tail);
        // ext2_buffer_freelist_tail = &(pb->list);
    }
    if (pb->b_flags & EXT2_BUFFER_WRITE_IMME && pb->b_dirt && pb->b_dev != 0) {
        ext2_rw_buffer(WRITE, pb);
    }
}

static void rw_ext2_blocks(int rw_flag, int dev, int block_nr, int block_count,
                           void* buf)
{
    if (!block_nr) return;

    ext2_superblock_t* psb = get_ext2_super_block(dev);
    unsigned long block_size = psb->sb_block_size;

    if (rw_flag == READ) {
        if (bdev_read(dev, block_size * block_nr, buf, block_size * block_count,
                      SELF) < 0)
            printl("ext2: warning: bdev_read failed!\n");
    } else {
        if (bdev_write(dev, block_size * block_nr, buf,
                       block_size * block_count, SELF) < 0)
            printl("ext2: warning: bdev_read failed!\n");
    }
}

static void ext2_rw_buffer(int rw_flag, ext2_buffer_t* pb)
{
    /*if (rw_flag == BDEV_WRITE) {
        memcpy(ext2fsbuf, pb->b_data, pb->b_size);
    }*/

    rw_ext2_blocks(rw_flag, pb->b_dev, pb->b_block, 1, pb->b_data);

    /*if (rw_flag == BDEV_READ) {
        memcpy(pb->b_data, ext2fsbuf, pb->b_size);
    }*/

    pb->b_dirt = 0;
}

/**
 * Clear the buffer.
 * @param pb Ptr to the buffer.
 */
void ext2_zero_buffer(ext2_buffer_t* pb)
{
    if (pb == NULL || pb->b_data == NULL) return;
    memset(pb->b_data, 0, pb->b_size);
}

/**
 * @brief sync_buffers Synchronize all dirty buffers
 *
 * @return
 */
void ext2_sync_buffers()
{
    ext2_buffer_t* pb;
    list_for_each_entry(pb, &ext2_buffer_freelist, list)
    {
        if (pb->b_dirt) {
#ifdef EXT2_BUFFER_DEBUG
            printl("Writing block #%d at dev 0x%x\n", pb->b_block, pb->b_dev);
#endif
            ext2_rw_buffer(WRITE, pb);
        }
    }
}
