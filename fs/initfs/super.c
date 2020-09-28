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
#include <fcntl.h>

#include "proto.h"
#include "global.h"
#include "tar.h"

#include <libbdev/libbdev.h>
#include <libfsdriver/libfsdriver.h>

int initfs_readsuper(dev_t dev, int flags, struct fsdriver_node* node)
{
    char buf[512];
    struct posix_tar_header* header = (struct posix_tar_header*)buf;
    loff_t position;
    struct fsdriver_buffer* bp;
    size_t block;
    off_t block_off;
    size_t bytes_rdwt;
    int i, retval;

    initfs_headers_count = 0;
    position = 0;

    for (i = 0;; i++) {
        block = position / ARCH_PG_SIZE;
        block_off = position % ARCH_PG_SIZE;
        bytes_rdwt = min(ARCH_PG_SIZE - block_off, sizeof(buf));

        if ((retval = fsdriver_get_block(&bp, dev, block)) != 0) return retval;
        memcpy(buf, bp->data + block_off, bytes_rdwt);
        fsdriver_put_block(bp);

        if (header->name[0] == '\0') break;

        size_t size = initfs_getsize(header->size);
        initfs_headers[i] = position;
        position += ((size / 512) + 1) * 512;
        if (size % 512) position += 512;

        initfs_headers_count++;
    }

    /* fill result */
    node->fn_num = 0;
    node->fn_uid = 0;
    node->fn_gid = 0;
    node->fn_size = 0;
    node->fn_mode = I_DIRECTORY | S_IRWXU;

    return 0;
}
