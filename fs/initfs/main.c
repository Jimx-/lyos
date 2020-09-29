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
#include "proto.h"
#include "tar.h"
#include "global.h"

#include <libfsdriver/libfsdriver.h>

static int initfs_putinode(dev_t dev, ino_t num);
static int initfs_sync(void);

static const struct fsdriver initfs_fsd = {
    .root_num = 0,

    .fs_readsuper = initfs_readsuper,
    .fs_putinode = initfs_putinode,
    .fs_lookup = initfs_lookup,
    .fs_read = initfs_read,
    .fs_write = initfs_write,
    .fs_getdents = initfs_getdents,
    .fs_stat = initfs_stat,
    .fs_driver = fsdriver_driver,
    .fs_sync = initfs_sync,
};

static void init_initfs(void)
{
    printl("initfs: InitFS driver is running\n");

    fsdriver_init_buffer_cache(1024);
}

int main()
{
    init_initfs();

    return fsdriver_start(&initfs_fsd);
}

static int initfs_putinode(dev_t dev, ino_t num) { return 0; }

static int initfs_sync(void) { return 0; }
