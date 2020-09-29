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

#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <libfsdriver/libfsdriver.h>

#include "proto.h"

int fsdriver_start(const struct fsdriver* fsd)
{
    int retval = fsdriver_register(fsd);
    if (retval != 0) return retval;

    int reply;

    MESSAGE m;
    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &m);

        int txn_id = VFS_TXN_GET_ID(m.type);
        int msgtype = VFS_TXN_GET_TYPE(m.type);
        int src = m.source;
        reply = 1;

        switch (msgtype) {
        case FS_LOOKUP:
            m.u.m_fs_vfs_lookup_reply.status = fsdriver_lookup(fsd, &m);
            break;
        case FS_PUTINODE:
            m.RET_RETVAL = fsdriver_putinode(fsd, &m);
            break;
        case FS_MOUNTPOINT:
            m.RET_RETVAL = fsdriver_mountpoint(fsd, &m);
            break;
        case FS_READSUPER:
            m.RET_RETVAL = fsdriver_readsuper(fsd, &m);
            break;
        case FS_STAT:
            m.RETVAL = fsdriver_stat(fsd, &m);
            break;
        case FS_RDWT:
            m.u.m_vfs_fs_readwrite.status = fsdriver_readwrite(fsd, &m);
            break;
        case FS_CREATE:
            m.u.m_fs_vfs_create_reply.status = fsdriver_create(fsd, &m);
            break;
        case FS_MKDIR:
            m.u.m_fs_vfs_create_reply.status = fsdriver_mkdir(fsd, &m);
            break;
        case FS_FTRUNC:
            m.RET_RETVAL = fsdriver_ftrunc(fsd, &m);
            break;
        case FS_CHMOD:
            m.RET_RETVAL = fsdriver_chmod(fsd, &m);
            break;
        case FS_GETDENTS:
            m.u.m_vfs_fs_readwrite.status = fsdriver_getdents(fsd, &m);
            break;
        case FS_SYNC:
            m.RET_RETVAL = fsdriver_sync(fsd, &m);
            break;
        case FS_RDLINK:
            m.u.m_vfs_fs_readwrite.status = fsdriver_rdlink(fsd, &m);
            break;
        case FS_SYMLINK:
            m.RETVAL = fsdriver_symlink(fsd, &m);
            break;
        default:
            if (fsd->fs_other) {
                fsd->fs_other(&m);
                reply = 0;
            } else
                m.RET_RETVAL = ENOSYS;
            break;
        }

        /* reply */
        if (reply) {
            m.type = VFS_TXN_TYPE_ID(FSREQ_RET, txn_id);
            send_recv(SEND, src, &m);
        }

        /* if (fsd->fs_sync) fsd->fs_sync(); */
    }

    return 0;
}
