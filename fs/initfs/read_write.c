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
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <sys/dirent.h>
#include <sys/stat.h>
#include <asm/page.h>

#include "proto.h"
#include "global.h"
#include "archive.h"

#include <libfsdriver/libfsdriver.h>

static ssize_t initfs_rdwt(dev_t dev, ino_t num, int rw_flag,
                           struct fsdriver_data* data, loff_t rwpos,
                           size_t count)
{
    const struct initfs_entry* entry;
    size_t block;
    loff_t block_pos;
    off_t block_off;
    size_t bytes_rdwt, cum_io;
    struct fsdriver_buffer* bp;
    int retval;

    if (num >= initfs_entries_count) return -EINVAL;
    entry = &initfs_entries[num];

    if (rwpos < 0 || rwpos >= entry->size) return 0;
    if (count > entry->size - rwpos) count = entry->size - rwpos;

    block_pos = entry->data_offset + rwpos;
    cum_io = 0;

    while (count > 0) {
        block = block_pos / ARCH_PG_SIZE;
        block_off = block_pos % ARCH_PG_SIZE;
        bytes_rdwt = min(ARCH_PG_SIZE - block_off, count);

        if (rw_flag == READ)
            retval =
                fsdriver_get_block_ino(&bp, dev, block, num, rwpos + cum_io);
        else
            retval = fsdriver_get_block(&bp, dev, block);
        if (retval != 0) return -retval;

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
    return -EROFS;
}

static const char* archive_name(const char* name)
{
    while (*name == '/')
        name++;
    if (name[0] == '.' && name[1] == '/') name += 2;
    return name;
}

static int entry_type(const struct initfs_entry* entry)
{
    if (S_ISREG(entry->mode)) return DT_REG;
    if (S_ISLNK(entry->mode)) return DT_LNK;
    if (S_ISCHR(entry->mode)) return DT_CHR;
    if (S_ISBLK(entry->mode)) return DT_BLK;
    if (S_ISDIR(entry->mode)) return DT_DIR;
    if (S_ISFIFO(entry->mode)) return DT_FIFO;
    return DT_UNKNOWN;
}

ssize_t initfs_getdents(dev_t dev, ino_t num, struct fsdriver_data* data,
                        loff_t* ppos, size_t count)
{
#define GETDENTS_BUFSIZE (sizeof(struct dirent) + INITFS_NAME_MAX)
#define GETDENTS_ENTRIES 8
    static char getdents_buf[GETDENTS_BUFSIZE * GETDENTS_ENTRIES];
    struct fsdriver_dentry_list list;
    char dirname[INITFS_NAME_MAX];
    size_t name_len;
    const char* p;
    const char dot = '.';
    loff_t pos = *ppos, new_pos = *ppos;
    int i, retval;

    if (num >= initfs_entries_count) return -EINVAL;
    strlcpy(dirname, num ? archive_name(initfs_entries[num].name) : "",
            sizeof(dirname));
    name_len = strlen(dirname);
    while (name_len && dirname[name_len - 1] == '/')
        dirname[--name_len] = '\0';

    fsdriver_dentry_list_init(&list, data, count, getdents_buf,
                              sizeof(getdents_buf));

    for (i = 0; i < initfs_entries_count; i++) {
        const struct initfs_entry* entry = &initfs_entries[i];
        const char* entry_name = archive_name(entry->name);
        size_t dir_len = strlen(dirname);

        if (dir_len) {
            if (strncmp(dirname, entry_name, dir_len)) continue;
            if (entry_name[dir_len] == '\0' || entry_name[dir_len] == '/')
                p = entry_name + dir_len;
            else
                continue;
            if (*p == '/') p++;
        } else {
            p = entry_name;
        }

        if (pos > 0) {
            pos--;
            continue;
        }

        name_len = 0;
        while (p[name_len] && p[name_len] != '/')
            name_len++;

        if (p[name_len] == '/') {
            const char* rest = p + name_len;
            while (*rest == '/')
                rest++;
            if (*rest) continue;
        }

        if (i == num || !name_len) {
            /* parent directory itself */
            retval =
                fsdriver_dentry_list_add(&list, i, &dot, 1, entry_type(entry));
        } else {
            retval = fsdriver_dentry_list_add(&list, i, p, name_len,
                                              entry_type(entry));
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

ssize_t initfs_rdlink(dev_t dev, ino_t num, struct fsdriver_data* data,
                      size_t bytes, endpoint_t user_endpt)
{
    const struct initfs_entry* entry;
    int retval;

    if (num >= initfs_entries_count) return -EINVAL;
    entry = &initfs_entries[num];
    if (!S_ISLNK(entry->mode)) return -EINVAL;
    if (bytes > strlen(entry->link)) bytes = strlen(entry->link);
    retval = fsdriver_copyout(data, 0, (void*)entry->link, bytes);
    return retval == 0 ? (ssize_t)bytes : -retval;
}
