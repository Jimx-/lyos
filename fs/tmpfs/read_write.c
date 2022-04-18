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
#include <sys/types.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/list.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <asm/page.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

static ssize_t rw_chunk(struct tmpfs_inode* pin, loff_t position, size_t chunk,
                        off_t offset, int rw_flag, struct fsdriver_data* data,
                        off_t data_offset)
{
    char* page;
    ssize_t retval;

    retval = tmpfs_inode_getpage(pin, offset >> ARCH_PG_SHIFT, &page);
    if (retval) return retval;

    if (rw_flag == READ)
        retval = fsdriver_copyout(data, data_offset, &page[offset], chunk);
    else
        retval = fsdriver_copyin(data, data_offset, &page[offset], chunk);

    return retval;
}

static ssize_t tmpfs_rdwt(dev_t dev, ino_t num, int rw_flag,
                          struct fsdriver_data* data, loff_t rwpos,
                          size_t count)
{
    struct tmpfs_inode* pin;
    loff_t pos = rwpos;
    size_t nbytes = count, chunk, file_size, bytes_rdwt;
    int eof;
    off_t offset, data_offset;
    int retval = 0;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return -EINVAL;

    file_size = pin->size;
    data_offset = 0;
    bytes_rdwt = 0;

    while (nbytes > 0) {
        offset = pos % ARCH_PG_SIZE;
        chunk = min(ARCH_PG_SIZE - offset, nbytes);
        eof = 0;

        if (rw_flag == READ) {
            if (file_size < pos) break;

            if (chunk > file_size - pos) {
                eof = 1;
                chunk = file_size - pos;
            }
        }

        retval = rw_chunk(pin, pos, chunk, offset, rw_flag, data, data_offset);
        if (retval != OK) break;

        nbytes -= chunk;
        pos += chunk;
        bytes_rdwt += chunk;

        if (eof) break;

        data_offset += chunk;
    }

    if (rw_flag == WRITE) {
        if (S_ISREG(pin->mode) || S_ISDIR(pin->mode)) {
            if (pos > pin->size) pin->size = pos;
        }
    }

    if (retval == OK) {
        if (rw_flag == READ) {
            pin->update = ATIME;
        } else {
            pin->update = CTIME | MTIME;
        }

        return bytes_rdwt;
    }

    return -retval;
}

ssize_t tmpfs_read(dev_t dev, ino_t num, struct fsdriver_data* data,
                   loff_t rwpos, size_t count)
{
    return tmpfs_rdwt(dev, num, READ, data, rwpos, count);
}

ssize_t tmpfs_write(dev_t dev, ino_t num, struct fsdriver_data* data,
                    loff_t rwpos, size_t count)
{
    return tmpfs_rdwt(dev, num, WRITE, data, rwpos, count);
}

static int stat_type(struct tmpfs_inode* pin)
{
    if (S_ISREG(pin->mode))
        return DT_REG;
    else if (S_ISDIR(pin->mode))
        return DT_DIR;
    else if (S_ISBLK(pin->mode))
        return DT_BLK;
    else if (S_ISCHR(pin->mode))
        return DT_CHR;
    else if (S_ISLNK(pin->mode))
        return DT_LNK;
    else if (S_ISSOCK(pin->mode))
        return DT_SOCK;
    else if (S_ISFIFO(pin->mode))
        return DT_FIFO;

    return DT_UNKNOWN;
}

ssize_t tmpfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                       loff_t* ppos, size_t count)
{
#define GETDENTS_BUFSIZE 4096
    static char getdents_buf[GETDENTS_BUFSIZE];

    ssize_t retval = 0;
    // int done = 0;
    loff_t pos = *ppos, i = 2;
    struct tmpfs_inode* pin;
    struct tmpfs_dentry* dp;
    struct fsdriver_dentry_list list;

    pin = tmpfs_get_inode(dev, num);
    if (!pin) return -EINVAL;

    fsdriver_dentry_list_init(&list, data, count, getdents_buf,
                              sizeof(getdents_buf));

    list_for_each_entry(dp, &pin->children, list)
    {
        if (i >= pos) {
            retval = fsdriver_dentry_list_add(&list, dp->inode->num, dp->name,
                                              strlen(dp->name),
                                              stat_type(dp->inode));

            if (retval < 0) {
                tmpfs_put_inode(pin);
                return retval;
            }
            if (retval == 0) break;
        }

        i++;
    }

    if (retval >= 0 && (retval = fsdriver_dentry_list_finish(&list)) >= 0) {
        *ppos = i;
    }

    tmpfs_put_inode(pin);

    return retval;
}
