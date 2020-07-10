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

int main()
{
    printl("initfs: InitFS driver is running\n");

    MESSAGE m;

    int reply;

    while (1) {
        send_recv(RECEIVE_ASYNC, ANY, &m);

        int txn_id = VFS_TXN_GET_ID(m.type);
        int msgtype = VFS_TXN_GET_TYPE(m.type);
        int src = m.source;
        reply = 1;

        switch (msgtype) {
        case FS_LOOKUP:
            m.RET_RETVAL = initfs_lookup(&m);
            break;
        case FS_PUTINODE:
            break;
        case FS_READSUPER:
            m.RET_RETVAL = initfs_readsuper(&m);
            break;
        case FS_STAT:
            m.STRET = initfs_stat(&m);
            break;
        case FS_RDWT:
            m.RWRET = initfs_rdwt(&m);
            break;
        case FS_SYNC:
            break;
        default:
            m.RET_RETVAL = ENOSYS;
            break;
        }

        /* reply */
        if (reply) {
            m.type = VFS_TXN_TYPE_ID(FSREQ_RET, txn_id);
            send_recv(SEND, src, &m);
        }
    }

    return 0;
}
