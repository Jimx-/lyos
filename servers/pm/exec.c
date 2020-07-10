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
#include <errno.h>
#include "lyos/proc.h"
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include "global.h"
#include "proto.h"

int do_exec(MESSAGE* m)
{
    endpoint_t ep = m->source;
    struct pmproc* pmp = pm_endpt_proc(ep);
    if (!pmp) return EINVAL;

    MESSAGE msg;
    msg.type = PM_VFS_EXEC;
    msg.PATHNAME = m->PATHNAME;
    msg.NAME_LEN = m->NAME_LEN;
    msg.BUF = m->BUF;
    msg.BUF_LEN = m->BUF_LEN;
    msg.ENDPOINT = ep;

    send_recv(BOTH, TASK_FS, &msg);

    if (msg.RETVAL) return msg.RETVAL;

    pmp->frame_addr = msg.BUF;
    pmp->frame_size = msg.BUF_LEN;

    /* tell tracer */
    if (pmp->tracer != NO_TASK) {
        sig_proc(pmp, SIGTRAP, TRUE);
    }

    return 0;
}
