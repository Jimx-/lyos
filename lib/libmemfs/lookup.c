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
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include "libmemfs/libmemfs.h"

PRIVATE int memfs_parse_path(dev_t dev, ino_t start, ino_t root, char * pathname, int flags, struct memfs_inode ** result, off_t * offsetp);

PUBLIC int memfs_lookup(dev_t dev, char * pathname, ino_t start, ino_t root, int flags, off_t * offset, struct fsdriver_node * fn)
{
    off_t offsetp = 0;
    struct memfs_inode * pin = NULL;

    int retval = memfs_parse_path(dev, start, root, pathname, flags, &pin, &offsetp);

    /* report error and offset position */
    if (retval == ELEAVEMOUNT || retval == EENTERMOUNT) {
        *offset = offsetp;
        if (retval == EENTERMOUNT) {
            fn->fn_num = pin->i_num;
        }
        return retval;
    }

    if (retval) return retval;

    /* fill result */
    fn->fn_num = pin->i_num;
    fn->fn_uid = pin->i_stat.st_uid;
    fn->fn_gid = pin->i_stat.st_gid;
    fn->fn_size = pin->i_stat.st_size;
    fn->fn_mode = pin->i_stat.st_mode;
    fn->fn_device = pin->i_stat.st_device;

    return 0;
}

PRIVATE char * get_name(char * pathname, char string[NAME_MAX + 1])
{
    size_t len;
    char * cp, * end;

    cp = pathname;
    end = cp;

    while(end[0] != '\0' && end[0] != '/') end++;

    len = (size_t)(end - cp);

    if (len > NAME_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    if (len == 0) {
        strcpy(string, ".");
    } else {
        memcpy(string, cp, len);
        string[len] = '\0';
    }

    return end;
}

PRIVATE struct memfs_inode * memfs_advance(struct memfs_inode * parent, char * name)
{
    struct memfs_inode * node;

    if (!(parent->i_stat.st_mode & I_DIRECTORY)) return ENOTDIR;

    list_for_each_entry(node, &(parent->i_children), i_list) {
        if (strcmp(node->i_name, name) == 0) {
            return node;
        }
    }

    return NULL;
}

PRIVATE int memfs_parse_path(dev_t dev, ino_t start, ino_t root, char * pathname, int flags, struct memfs_inode ** result, off_t * offsetp)
{
    struct memfs_inode * pin, * dir_pin;
    char * cp, * next;
    char component[NAME_MAX + 1];

    cp = pathname;

    if (!(pin = memfs_find_inode(start))) return ENOENT;

    /* scan */
    while (1) {
        if (cp[0] == '\0') {
            *result = pin;
            *offsetp = cp - pathname;

            return 0;
        }

        while (cp[0] == '/') cp++;

        if (!(next = get_name(cp, component))) {
            return errno;
        }

        if (strcmp(component, "..") == 0) {
            /* climb up to top fs */
            if (pin->i_num == 0) {
                *offsetp = cp - pathname;
                return ELEAVEMOUNT;
            }
        }

        dir_pin = pin;
        pin = memfs_advance(dir_pin, component);
        if ((errno == EENTERMOUNT) || (errno == ELEAVEMOUNT)) errno = 0;

        if (!pin) return errno;

        /* next component */
        cp = next;
    }
}
