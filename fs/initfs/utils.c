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
#include "lyos/list.h"
#include <fcntl.h>
#include <asm/page.h>

#include "proto.h"
#include "global.h"
#include "tar.h"

#include <libfsdriver/libfsdriver.h>

unsigned int initfs_getsize(const char* in)
{

    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

unsigned int initfs_get8(const char* in)
{

    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 7; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

unsigned int initfs_getmode(const struct posix_tar_header* phdr)
{
    unsigned int mode = 0;

    switch (phdr->typeflag) {
    case REGTYPE:
    case AREGTYPE:
        mode = S_IFREG;
        break;
    case SYMTYPE:
        mode = S_IFLNK;
        break;
    case CHRTYPE:
        mode = S_IFCHR;
        break;
    case BLKTYPE:
        mode = S_IFBLK;
        break;
    case DIRTYPE:
        mode = S_IFDIR;
        break;
    case FIFOTYPE:
        mode = S_IFIFO;
        break;
    }

    mode |= initfs_get8(phdr->mode);

    return mode;
}

int initfs_read_header(dev_t dev, ino_t num, char* header, size_t header_size)
{
    struct fsdriver_buffer* bp;
    size_t header_block;
    off_t block_off;
    size_t bytes_rdwt;
    int retval;

    header_block = initfs_headers[num] / ARCH_PG_SIZE;
    block_off = initfs_headers[num] % ARCH_PG_SIZE;
    bytes_rdwt = min(ARCH_PG_SIZE - block_off, header_size);

    if ((retval = fsdriver_get_block(&bp, dev, header_block)) != 0)
        return retval;
    memcpy(header, bp->data + block_off, bytes_rdwt);
    fsdriver_put_block(bp);

    assert(bytes_rdwt == header_size);

    return 0;
}
