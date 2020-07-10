/*
    (c)Copyright 2011 Jimx

    This file is part of Lyos.

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
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/service.h>
#include "libmemfs/libmemfs.h"
#include "global.h"
#include "proto.h"

static int init_procfs();
static int procfs_init_hook();
int procfs_lookup_hook();

struct memfs_hooks fs_hooks = {
    .init_hook = procfs_init_hook,
    .read_hook = procfs_read_hook,
    .getdents_hook = NULL,
    .lookup_hook = procfs_lookup_hook,
};

int main()
{
    serv_register_init_fresh_callback(init_procfs);
    serv_init();

    struct memfs_stat root_stat;
    root_stat.st_mode = (I_DIRECTORY | 0755);
    root_stat.st_uid = SU_UID;
    root_stat.st_gid = 0;

    return memfs_start("proc", &fs_hooks, &root_stat);
}

static int init_procfs()
{
    printl("procfs: procfs is running.\n");

    return 0;
}

static void build_root(struct memfs_inode* root)
{
    struct memfs_stat stat;

    stat.st_uid = SU_UID;
    stat.st_gid = 0;
    stat.st_size = 0;

    struct procfs_file* fp;
    for (fp = root_files; fp->name != NULL; fp++) {
        stat.st_mode = fp->mode;
        struct memfs_inode* pin =
            memfs_add_inode(root, fp->name, NO_INDEX, &stat, fp->data);
        if (pin == NULL) return;
    }
}

static int procfs_init_hook()
{
    static int first = 1;

    if (first) {
        struct memfs_inode* root = memfs_get_root_inode();

        build_root(root);

        first = 0;
    }

    return 0;
}
