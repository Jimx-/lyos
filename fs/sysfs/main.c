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
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include <libsysfs/libsysfs.h>
#include "node.h"
#include "proto.h"

PRIVATE int sysfs_init();
PRIVATE int sysfs_message_hook(MESSAGE * m);

struct memfs_hooks fs_hooks = {
    .init_hook = NULL,
    .message_hook = sysfs_message_hook,
    .read_hook = sysfs_read_hook,
    .write_hook = sysfs_write_hook,
    .getdents_hook = NULL,
};

PUBLIC int main()
{
    sysfs_init();

    struct memfs_stat root_stat;
    root_stat.st_mode = (I_DIRECTORY | 0755);
    root_stat.st_uid = SU_UID;
    root_stat.st_gid = 0;

    return memfs_start(NULL, &fs_hooks, &root_stat);
}

PRIVATE int sysfs_init()
{
    printl("sysfs: SysFS is running\n");

    init_node();

    return 0;
}

PRIVATE int sysfs_message_hook(MESSAGE * m)
{
    int msgtype = m->type;
    int retval = 0;
    
    switch (msgtype) {
    case SYSFS_PUBLISH:
        retval = do_publish(m); 
        break;
    case SYSFS_RETRIEVE:
        retval = do_retrieve(m);
        break;
    default:
        return ENOSYS;
    }

    return retval;
}
