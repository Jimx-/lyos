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
#include <sys/dirent.h>

#include "proto.h"
#include "global.h"
#include "tar.h"

#include <libfsdriver/libfsdriver.h>

static ssize_t initfs_rdwt(dev_t dev, ino_t num, int rw_flag,
                           struct fsdriver_data* data, loff_t rwpos,
                           size_t count)
{
    char header[512];
    struct posix_tar_header* phdr = (struct posix_tar_header*)header;
    size_t block;
    loff_t block_pos;
    off_t block_off;
    size_t bytes_rdwt, cum_io;
    struct fsdriver_buffer* bp;
    int retval;

    if (num >= initfs_headers_count) return -EINVAL;

    if ((retval = initfs_read_header(dev, num, header, sizeof(header))) != 0)
        return -retval;

    off_t filesize = initfs_getsize(phdr->size);

    if (filesize < rwpos + count) {
        count = filesize - rwpos;
    }

    block_pos = initfs_headers[num] + 512 + rwpos;
    cum_io = 0;

    while (count > 0) {
        block = block_pos / ARCH_PG_SIZE;
        block_off = block_pos % ARCH_PG_SIZE;
        bytes_rdwt = min(ARCH_PG_SIZE - block_off, count);

        if ((retval = fsdriver_get_block(&bp, dev, block)) != 0) return -retval;

        if (rw_flag == READ) {
            if ((retval = fsdriver_copyout(data, cum_io, bp->data + block_off,
                                           bytes_rdwt)) != 0)
                return -retval;
        } else {
            if ((retval = fsdriver_copyin(data, cum_io, bp->data + block_off,
                                          bytes_rdwt)) != 0)
                return -retval;
            fsdriver_mark_dirty(bp);
        }

        fsdriver_put_block(bp);

        block_pos += bytes_rdwt;
        count -= bytes_rdwt;
        cum_io += bytes_rdwt;
    }

    return cum_io;
}

ssize_t initfs_read(dev_t dev, ino_t num, struct fsdriver_data* data,
                    loff_t rwpos, size_t count)
{
    return initfs_rdwt(dev, num, READ, data, rwpos, count);
}

ssize_t initfs_write(dev_t dev, ino_t num, struct fsdriver_data* data,
                     loff_t rwpos, size_t count)
{
    return initfs_rdwt(dev, num, WRITE, data, rwpos, count);
}

static int match_dirname(const char* dirname, const char* pathname)
{
    size_t dir_len, path_len;
    const char* path_lim;

    dir_len = strlen(dirname);
    path_len = strlen(pathname);
    path_lim = pathname + path_len;

    if (path_len < dir_len) {
        return FALSE;
    }

    if (memcmp(dirname, pathname, dir_len)) return FALSE;

    pathname += dir_len;

    while (path_lim > pathname && *(path_lim - 1) == '/')
        path_lim--;

    while (pathname < path_lim)
        if (*pathname++ == '/') return FALSE;

    return TRUE;
}

static int header_type(const struct posix_tar_header* phdr)
{
    switch (phdr->typeflag) {
    case REGTYPE:
    case AREGTYPE:
        return DT_REG;
    case SYMTYPE:
        return DT_LNK;
    case CHRTYPE:
        return DT_CHR;
    case BLKTYPE:
        return DT_BLK;
    case DIRTYPE:
        return DT_DIR;
    case FIFOTYPE:
        return DT_FIFO;
    }

    return DT_UNKNOWN;
}

ssize_t initfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                        loff_t* ppos, size_t count)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + TAR_MAX_PATH)
#define GETDENTS_ENTRIES 8
    static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
    struct fsdriver_dentry_list list;
    char header[512];
    struct posix_tar_header* phdr = (struct posix_tar_header*)header;
    char dirname[TAR_MAX_PATH];
    size_t name_len;
    char* p;
    const char dot = '.';
    loff_t pos = *ppos, new_pos = *ppos;
    int i, retval;

    if (num >= initfs_headers_count) return -EINVAL;

    if ((retval = initfs_read_header(dev, num, header, sizeof(header))) != 0)
        return -retval;
    strlcpy(dirname, phdr->name, TAR_MAX_PATH);

    fsdriver_dentry_list_init(&list, data, count, getdents_buf,
                              sizeof(getdents_buf));

    for (i = 0; i < initfs_headers_count; i++) {
        if ((retval = initfs_read_header(dev, i, header, sizeof(header))) != 0)
            return -retval;

        if (!match_dirname(dirname, phdr->name)) continue;

        if (pos > 0) {
            pos--;
            continue;
        }

        p = phdr->name;
        p += strlen(dirname);

        name_len = 0;
        while (p[name_len] && p[name_len] != '/')
            name_len++;

        if (!name_len) {
            /* parent directory itself */
            retval =
                fsdriver_dentry_list_add(&list, i, &dot, 1, header_type(phdr));
        } else {
            retval = fsdriver_dentry_list_add(&list, i, p, name_len - 1,
                                              header_type(phdr));
        }

        if (retval < 0) return retval;
        if (retval == 0) break;

        new_pos++;
    }

    if (retval >= 0 && (retval = fsdriver_dentry_list_finish(&list)) >= 0) {
        *ppos = new_pos;
    }

    return retval;
}
