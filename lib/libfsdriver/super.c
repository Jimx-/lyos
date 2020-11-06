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
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/fs.h>
#include <string.h>
#include <sys/syslimits.h>
#include <lyos/sysutils.h>
#include <asm/page.h>

#include <libbdev/libbdev.h>
#include <libfsdriver/libfsdriver.h>

#define ENOPARAM (-400)

struct sb_flag_entry {
    const char* name;
    int flag;
};

static int copy_options(endpoint_t src, mgrant_id_t grant, size_t data_size,
                        void** optsp)
{
    void* options;
    int retval;

    *optsp = NULL;

    if (!data_size) return 0;

    options = malloc(ARCH_PG_SIZE);
    if (!options) return ENOMEM;

    retval = safecopy_from(src, grant, 0, options, data_size);
    if (retval) goto err;

    if (data_size < ARCH_PG_SIZE) {
        if (safecopy_from(src, grant, data_size, options + data_size,
                          ARCH_PG_SIZE - data_size))
            memset(options + data_size, 0, ARCH_PG_SIZE - data_size);
    }

    ((char*)options)[ARCH_PG_SIZE - 1] = 0;

    *optsp = options;
    return 0;

err:
    free(options);
    return retval;
}

int fsdriver_readsuper(const struct fsdriver* fsd, MESSAGE* m)
{
    struct fsdriver_node fn;
    endpoint_t src = m->source;
    dev_t dev = m->u.m_vfs_fs_readsuper.dev;
    int flags = m->u.m_vfs_fs_readsuper.flags;
    mgrant_id_t grant = m->u.m_vfs_fs_readsuper.grant;
    size_t data_size = m->u.m_vfs_fs_readsuper.data_size;
    struct fsdriver_context ctx;
    void* options;
    int retval;

    if (fsd->fs_readsuper == NULL) return ENOSYS;

    retval = copy_options(src, grant, data_size, &options);
    if (retval) return retval;

    if (fsd->fs_driver) fsd->fs_driver(dev);

    ctx.sb_flags = flags;

    retval = fsd->fs_readsuper(dev, &ctx, options, &fn);
    if (retval) goto out;

    memset(m, 0, sizeof(MESSAGE));
    m->u.m_fs_vfs_create_reply.num = fn.fn_num;
    m->u.m_fs_vfs_create_reply.mode = fn.fn_mode;
    m->u.m_fs_vfs_create_reply.size = fn.fn_size;
    m->u.m_fs_vfs_create_reply.uid = fn.fn_uid;
    m->u.m_fs_vfs_create_reply.gid = fn.fn_gid;

out:
    free(options);
    return retval;
}

static const struct sb_flag_entry sb_set_flag[] = {
    {"ro", RF_READONLY},
    {NULL, 0},
};

static const struct sb_flag_entry sb_clear_flag[] = {
    {"rw", RF_READONLY},
    {NULL, 0},
};

static int fsdriver_parse_sb_flag(struct fsdriver_context* fc, const char* key)
{
    const struct sb_flag_entry* entry;

    for (entry = sb_set_flag; entry->name; entry++) {
        if (!strcmp(entry->name, key)) {
            fc->sb_flags |= entry->flag;
            return 0;
        }
    }

    for (entry = sb_clear_flag; entry->name; entry++) {
        if (!strcmp(entry->name, key)) {
            fc->sb_flags &= ~entry->flag;
            return 0;
        }
    }

    return ENOPARAM;
}

int fsdriver_parse_param(struct fsdriver_context* fc, const char* key,
                         const char* value, size_t len,
                         int (*fs_parser)(struct fsdriver_context* fc,
                                          const char* key, const char* value,
                                          size_t len))
{
    int retval;

    retval = fsdriver_parse_sb_flag(fc, key);
    if (retval != ENOPARAM) return retval;

    return fs_parser(fc, key, value, len);
}
