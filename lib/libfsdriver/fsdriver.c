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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include <lyos/fs.h>
#include "string.h"
#include <lyos/ipc.h>
#include "libfsdriver/libfsdriver.h"

PUBLIC int fsdriver_start(struct fsdriver * fsd)
{
    int retval = fsdriver_register(fsd);
    if (retval != 0) return retval;

    int reply;

    MESSAGE m;
    while (TRUE) {
        send_recv(RECEIVE, ANY, &m);

        int msgtype = m.type;
        int src = m.source;
        reply = 1;

        switch(msgtype) {
        case FS_LOOKUP:
            m.RET_RETVAL = fsdriver_lookup(fsd, &m);
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
            m.STRET = fsdriver_stat(fsd, &m);
            break;
        case FS_RDWT:
            m.RWRET = fsdriver_readwrite(fsd, &m);
            break;
        case FS_CREATE:
            m.CRRET = fsdriver_create(fsd, &m);
            break;
        case FS_FTRUNC:
            m.RET_RETVAL = fsdriver_ftrunc(fsd, &m);
            break;
        case FS_CHMOD:
            m.RET_RETVAL = fsdriver_chmod(fsd, &m);
            break;
        case FS_GETDENTS:
            m.RET_RETVAL = fsdriver_getdents(fsd, &m);
            break;
        case FS_SYNC:
            m.RET_RETVAL = fsdriver_sync(fsd, &m);
            break;
        default:
            m.RET_RETVAL = ENOSYS;
            break;
        }

        /* reply */
        if (reply) {
            m.type = FSREQ_RET;
            send_recv(SEND, src, &m);
        }

        fsd->fs_sync();
    }

    return 0;
}
