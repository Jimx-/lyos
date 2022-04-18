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
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/list.h>
#include <lyos/sysutils.h>
#include <sys/stat.h>
#include <ctype.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "types.h"
#include "proto.h"

struct tmpfs_options {
    mode_t mode;
    uid_t uid;
    gid_t gid;
};

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

static inline void tmpfs_init_options(struct tmpfs_options* opts)
{
    opts->mode = S_ISVTX | 0777;
    opts->uid = SU_UID;
    opts->gid = 0;
}

static int tmpfs_parse_one(struct fsdriver_context* fc, const char* key,
                           const char* value, size_t len)
{
    struct tmpfs_options* opts = fc->fs_private;

    if (!strcmp(key, "mode")) {
        char* endptr;
        mode_t mode;
        mode = strtol(value, &endptr, 8);

        if (*endptr) return EINVAL;

        opts->mode = mode & 07777;
        return 0;
    }

    return EINVAL;
}

static int tmpfs_parse_options(struct fsdriver_context* fc, void* data)
{
    char* optstr = data;

    while (optstr != NULL) {
        char* key = optstr;

        for (;;) {
            optstr = strchr(optstr, ',');
            if (optstr == NULL) break;

            optstr++;
            if (!isdigit((int)*optstr)) {
                optstr[-1] = '\0';
                break;
            }
        }

        if (*key) {
            char* value = strchr(key, '=');
            size_t len = 0;
            int retval;

            if (value) {
                *value++ = '\0';
                len = strlen(value);
            }
            retval = fsdriver_parse_param(fc, key, value, len, tmpfs_parse_one);
            if (retval) return retval;
        }
    }

    return 0;
}

int tmpfs_readsuper(dev_t dev, struct fsdriver_context* fc, void* data,
                    struct fsdriver_node* node)
{
    struct tmpfs_superblock* sb;
    struct tmpfs_inode* root;
    struct tmpfs_options opts;
    int retval;
    int readonly;

    tmpfs_init_options(&opts);
    fc->fs_private = &opts;

    retval = tmpfs_parse_options(fc, data);
    if (retval) return retval;

    sb = alloc_superblock(dev);
    if (!sb) return ENOMEM;

    retval = ENOMEM;
    root = tmpfs_alloc_inode(sb, TMPFS_ROOT_INODE);
    if (!root) goto out_free_sb;

    readonly = !!(fc->sb_flags & RF_READONLY);
    sb->readonly = readonly;

    root->mode = S_IFDIR | opts.mode;
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
