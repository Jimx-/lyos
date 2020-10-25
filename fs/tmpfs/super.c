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
#include <fcntl.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

static DEF_LIST(superblock_list);

static struct tmpfs_superblock* alloc_superblock(dev_t dev)
{
    struct tmpfs_superblock* sb;

    sb = malloc(sizeof(*sb));
    if (!sb) return NULL;

    memset(sb, 0, sizeof(*sb));
    sb->dev = dev;
    INIT_LIST_HEAD(&sb->list);

    return sb;
}

static void free_superblock(struct tmpfs_superblock* sb) { free(sb); }

struct tmpfs_superblock* tmpfs_get_superblock(dev_t dev)
{
    struct tmpfs_superblock* sb;

    list_for_each_entry(sb, &superblock_list, list)
    {
        if (sb->dev == dev) return sb;
    }

    return NULL;
}

int tmpfs_readsuper(dev_t dev, int flags, struct fsdriver_node* node)
{
    struct tmpfs_superblock* sb;
    struct tmpfs_inode* root;
    int retval;
    int readonly;

    sb = alloc_superblock(dev);
    if (!sb) return ENOMEM;

    retval = ENOMEM;
    root = tmpfs_alloc_inode(sb, TMPFS_ROOT_INODE);
    if (!root) goto out_free_sb;

    readonly = !!(flags & RF_READONLY);
    sb->readonly = readonly;

    root->mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    root->uid = 0;
    root->gid = 0;
    root->spec_dev = NO_DEV;
    root->atime = root->ctime = root->mtime = now();

    root->link_count++;
    tmpfs_search_dir(root, ".", &root, SD_MAKE);
    root->link_count++;
    tmpfs_search_dir(root, "..", &root, SD_MAKE);

    list_add(&sb->list, &superblock_list);

    /* fill result */
    node->fn_num = root->num;
    node->fn_uid = root->uid;
    node->fn_gid = root->gid;
    node->fn_size = root->size;
    node->fn_mode = root->mode;

    return 0;

out_free_sb:
    free_superblock(sb);

    return retval;
}
