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
#include "lyos/list.h"
#include <sys/stat.h>
#include "libmemfs/libmemfs.h"
#include <libsysfs/libsysfs.h>
#include "node.h"
#include "proto.h"

static int sysfs_init();
static void sysfs_message_hook(MESSAGE* m);

struct memfs_hooks fs_hooks = {
    .message_hook = sysfs_message_hook,
    .read_hook = sysfs_read_hook,
    .write_hook = sysfs_write_hook,
    .rdlink_hook = sysfs_rdlink_hook,
};

int main()
{
    sysfs_init();

    struct memfs_stat root_stat;
    root_stat.st_mode =
        S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    root_stat.st_uid = SU_UID;
    root_stat.st_gid = 0;

    return memfs_start(NULL, &fs_hooks, &root_stat);
}

static int sysfs_init()
{
    printl("sysfs: SysFS is running\n");

    init_node();

    return 0;
}

static void sysfs_message_hook(MESSAGE* m)
{
    int msgtype = m->type;

    switch (msgtype) {
    case SYSFS_PUBLISH:
        m->u.m_sysfs_req.status = do_publish(m);
        break;
    case SYSFS_PUBLISH_LINK:
        m->u.m_sysfs_req.status = do_publish_link(m);
        break;
    case SYSFS_RETRIEVE:
        m->u.m_sysfs_req.status = do_retrieve(m);
        break;
    case SYSFS_SUBSCRIBE:
        m->u.m_sysfs_req.status = do_subscribe(m);
        break;
    case SYSFS_GET_EVENT:
        m->u.m_sysfs_req.status = do_get_event(m);
        break;
    default:
        m->RETVAL = ENOSYS;
        break;
    }

    if (m->RETVAL != SUSPEND) {
        m->type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, m->source, m);
    }
}
