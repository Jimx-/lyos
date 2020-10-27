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
#include <lyos/config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/list.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <sys/stat.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

#define BOGO_DIRENT_SIZE 20

static inline struct tmpfs_dentry* alloc_dentry(const char* name,
                                                struct tmpfs_inode* pin)
{
    struct tmpfs_dentry* dp;

    dp = malloc(sizeof(*dp));
    if (!dp) return NULL;

    strlcpy(dp->name, name, sizeof(dp->name));
    dp->inode = pin;
    INIT_LIST_HEAD(&dp->list);

    return dp;
}

static inline void free_dentry(struct tmpfs_dentry* dp) { free(dp); }

int tmpfs_advance(struct tmpfs_inode* parent, const char* name,
                  struct tmpfs_inode** ppin)
{
    int retval;

    if (!parent) return EINVAL;

    if (name[0] == '\0') return ENOENT;

    if (!S_ISDIR(parent->mode)) return ENOTDIR;

    if ((retval = tmpfs_search_dir(parent, name, ppin, SD_LOOK_UP)) != OK)
        return retval;

    tmpfs_dup_inode(*ppin);

    return 0;
}

int tmpfs_search_dir(struct tmpfs_inode* dir_pin, const char* string,
                     struct tmpfs_inode** ppin, int flag)
{
    struct tmpfs_dentry *dp, *tmp;
    int match;
    int retval = 0;

    if (!S_ISDIR(dir_pin->mode)) return ENOTDIR;

    list_for_each_entry_safe(dp, tmp, &dir_pin->children, list)
    {
        match = 0;

        if (flag != SD_MAKE) {
            if (flag == SD_IS_EMPTY)
                match = strcmp(dp->name, ".") && strcmp(dp->name, "..");
            else
                match = !strcmp(dp->name, string);
        }

        retval = 0;
        if (match) {
            if (flag == SD_IS_EMPTY)
                retval = ENOTEMPTY;
            else if (flag == SD_DELETE) {
                list_del(&dp->list);
                free_dentry(dp);

                dir_pin->size -= BOGO_DIRENT_SIZE;
                dir_pin->update |= CTIME | MTIME;
            } else {
                *ppin = dp->inode;
            }

            return retval;
        }
    }

    if (flag != SD_MAKE) return flag == SD_IS_EMPTY ? 0 : ENOENT;

    dp = alloc_dentry(string, *ppin);
    list_add(&dp->list, &dir_pin->children);

    dir_pin->size += BOGO_DIRENT_SIZE;
    dir_pin->update |= CTIME | MTIME;

    return 0;
}

int tmpfs_lookup(dev_t dev, ino_t start, const char* name,
                 struct fsdriver_node* fn, int* is_mountpoint)
{
    struct tmpfs_inode *dir_pin, *pin;
    int retval;

    dir_pin = tmpfs_get_inode(dev, start);
    if (!dir_pin) return EINVAL;

    retval = tmpfs_advance(dir_pin, name, &pin);
    if (retval) goto out;

    /* fill result */
    fn->fn_num = pin->num;
    fn->fn_uid = pin->uid;
    fn->fn_gid = pin->gid;
    fn->fn_size = pin->size;
    fn->fn_mode = pin->mode;
    fn->fn_device = pin->spec_dev;
    *is_mountpoint = pin->mountpoint;

out:
    tmpfs_put_inode(dir_pin);

    return retval;
}
