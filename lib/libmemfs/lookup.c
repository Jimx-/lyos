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
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/const.h>
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include "proto.h"

static struct memfs_inode* memfs_advance(struct memfs_inode* parent,
                                         const char* name);

int memfs_lookup(dev_t dev, ino_t start, const char* name,
                 struct fsdriver_node* fn, int* is_mountpoint)
{
    struct memfs_inode* dir_pin = memfs_find_inode(start);
    if (!dir_pin) return EINVAL;

    struct memfs_inode* pin = memfs_advance(dir_pin, name);
    if (!pin) return errno;

    /* fill result */
    fn->fn_num = pin->i_num;
    fn->fn_uid = pin->i_stat.st_uid;
    fn->fn_gid = pin->i_stat.st_gid;
    fn->fn_size = pin->i_stat.st_size;
    fn->fn_mode = pin->i_stat.st_mode;
    fn->fn_device = pin->i_stat.st_device;
    *is_mountpoint = 0;

    return 0;
}

static struct memfs_inode* memfs_advance(struct memfs_inode* parent,
                                         const char* name)
{
    struct memfs_inode* node;

    if (!S_ISDIR(parent->i_stat.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    if (!strcmp(name, ".")) return parent;
    if (*name == '\0') return parent;

    if (!strcmp(name, "..")) {
        if (parent->i_parent == NULL) {
            errno = ENOENT;
            return NULL;
        }

        return parent->i_parent;
    }

    if (fs_hooks.lookup_hook) {
        int r;
        if ((r = fs_hooks.lookup_hook(parent, name, parent->data)) != 0) {
            errno = r;
            return NULL;
        }
    }

    list_for_each_entry(node, &(parent->i_children), i_list)
    {
        if (strcmp(node->i_name, name) == 0) {
            return node;
        }
    }

    errno = ENOENT;
    return NULL;
}
