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
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include <libfsdriver/libfsdriver.h>

#include "proto.h"

void fsdriver_process(const struct fsdriver* fsd, MESSAGE* msg)
{
    int txn_id = VFS_TXN_GET_ID(msg->type);
    int msgtype = VFS_TXN_GET_TYPE(msg->type);
    int src = msg->source;
    int reply = TRUE;

    switch (msgtype) {
    case FS_LOOKUP:
        msg->u.m_fs_vfs_lookup_reply.status = fsdriver_lookup(fsd, msg);
        break;
    case FS_PUTINODE:
        msg->RETVAL = fsdriver_putinode(fsd, msg);
        break;
    case FS_MOUNTPOINT:
        msg->RET_RETVAL = fsdriver_mountpoint(fsd, msg);
        break;
    case FS_READSUPER:
        msg->u.m_fs_vfs_create_reply.status = fsdriver_readsuper(fsd, msg);
        break;
    case FS_STAT:
        msg->RETVAL = fsdriver_stat(fsd, msg);
        break;
    case FS_RDWT:
        msg->u.m_vfs_fs_readwrite.status = fsdriver_readwrite(fsd, msg);
        break;
    case FS_CREATE:
        msg->u.m_fs_vfs_create_reply.status = fsdriver_create(fsd, msg);
        break;
    case FS_MKDIR:
        msg->u.m_fs_vfs_create_reply.status = fsdriver_mkdir(fsd, msg);
        break;
    case FS_MKNOD:
        msg->RETVAL = fsdriver_mknod(fsd, msg);
        break;
    case FS_FTRUNC:
        msg->RET_RETVAL = fsdriver_ftrunc(fsd, msg);
        break;
    case FS_CHMOD:
        msg->RET_RETVAL = fsdriver_chmod(fsd, msg);
        break;
    case FS_CHOWN:
        msg->u.m_vfs_fs_chown.status = fsdriver_chown(fsd, msg);
        break;
    case FS_GETDENTS:
        msg->u.m_vfs_fs_readwrite.status = fsdriver_getdents(fsd, msg);
        break;
    case FS_SYNC:
        msg->RET_RETVAL = fsdriver_sync(fsd, msg);
        break;
    case FS_RDLINK:
        msg->u.m_vfs_fs_readwrite.status = fsdriver_rdlink(fsd, msg);
        break;
    case FS_LINK:
        msg->RETVAL = fsdriver_link(fsd, msg);
        break;
    case FS_SYMLINK:
        msg->RETVAL = fsdriver_symlink(fsd, msg);
        break;
    case FS_UNLINK:
        msg->RETVAL = fsdriver_unlink(fsd, msg);
        break;
    case FS_RMDIR:
        msg->RETVAL = fsdriver_rmdir(fsd, msg);
        break;
    case FS_UTIME:
        msg->RETVAL = fsdriver_utime(fsd, msg);
        break;
    default:
        if (fsd->fs_other) {
            fsd->fs_other(msg);
            reply = 0;
        } else
            msg->RET_RETVAL = ENOSYS;
        break;
    }

    /* reply */
    if (reply) {
        msg->type = VFS_TXN_TYPE_ID(FSREQ_RET, txn_id);
        asyncsend3(src, msg, 0);
    }

    /* if (fsd->fs_sync) fsd->fs_sync(); */
}

int fsdriver_start(const struct fsdriver* fsd)
{
    MESSAGE m;
    int retval;

    retval = fsdriver_register(fsd);
    if (retval != 0) return retval;

    while (TRUE) {
        send_recv(RECEIVE_ASYNC, ANY, &m);

        fsdriver_process(fsd, &m);
    }

    return 0;
}
