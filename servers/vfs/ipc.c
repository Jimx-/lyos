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
#include <sys/types.h>
#include <lyos/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sched.h>
#include "types.h"
#include "path.h"
#include "global.h"
#include "proto.h"
#include "global.h"
#include "thread.h"

static int sendmsg(endpoint_t dest, struct worker_thread* wp)
{
    int txn_id;

    txn_id = wp->tid + FS_TXN_ID;
    wp->msg_recv->type = VFS_TXN_TYPE_ID(wp->msg_recv->type, txn_id);
    wp->recv_from = dest;

    return asyncsend3(dest, wp->msg_recv, 0);
}

int fs_sendrec(endpoint_t fs_e, MESSAGE* msg)
{
    int retval;
    if (fs_e == fproc->endpoint) {
        return EDEADLK;
    }

    self->msg_recv = msg;

    retval = sendmsg(fs_e, self);
    if (retval) {
        return retval;
    }

    worker_wait();

    return 0;
}
