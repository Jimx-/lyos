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

#include <lyos/ipc.h>
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include <sys/stat.h>

#include "proto.h"
#include "global.h"
#include "tar.h"

static void fill_node(ino_t num, struct fsdriver_node* fn,
                      const struct posix_tar_header* phdr)
{
    dev_t major, minor;

    fn->fn_num = num;
    fn->fn_uid = initfs_get8(phdr->uid);
    fn->fn_gid = initfs_get8(phdr->gid);
    fn->fn_size = initfs_getsize(phdr->size);

    if (!num) {
        /* special hack for root inode */
        fn->fn_mode = S_IFDIR | S_IRWXU;
    } else {
        fn->fn_mode = initfs_getmode(phdr);
    }

    major = initfs_get8(phdr->devmajor);
    minor = initfs_get8(phdr->devminor);
    fn->fn_device = MAKE_DEV(major, minor);
}

int initfs_lookup(dev_t dev, ino_t start, const char* name,
                  struct fsdriver_node* fn, int* is_mountpoint)
{
    char header[512];
    struct posix_tar_header* phdr = (struct posix_tar_header*)header;
    char string[TAR_MAX_PATH], *p, filename[TAR_MAX_PATH];
    size_t base_len, name_len;
    int i, retval;

    *is_mountpoint = FALSE;

    if ((retval = initfs_read_header(dev, start, header, sizeof(header))) != 0)
        return retval;

    if (!strcmp(name, ".")) {
        fill_node(start, fn, phdr);
        return 0;
    }

    if (!start) {
        /* do not include the file name for root inode */
        base_len = 0;
    } else {
        /* include the parent name */
        base_len = strlen(phdr->name);
        strlcpy(string, phdr->name, TAR_MAX_PATH);
    }

    strlcpy(string + base_len, name, TAR_MAX_PATH - base_len);

    /* skip leading slashes */
    p = string;
    while (*p && *p == '/')
        p++;

    for (i = 0; i < initfs_headers_count; i++) {
        if ((retval = initfs_read_header(dev, i, header, sizeof(header))) != 0)
            return retval;

        strlcpy(filename, phdr->name, TAR_MAX_PATH);
        name_len = strlen(filename);

        /* remove trailing slashes */
        while (name_len && filename[name_len - 1] == '/') {
            filename[name_len - 1] = '\0';
            name_len--;
        }

        if (!strcmp(p, filename)) {
            fill_node(i, fn, phdr);
            return 0;
        }
    }

    return ENOENT;
}
