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
#include <lyos/fs.h>
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include <signal.h>

#include "pmproc.h"
#include "proto.h"

int do_exec(MESSAGE* m)
{
    endpoint_t ep = m->source;
    struct pmproc* pmp = pm_endpt_proc(ep);
    MESSAGE msg;
    struct vfs_exec_request* exec_req;
    struct vfs_exec_response* exec_resp;
    int sugid;
    uid_t new_uid;
    gid_t new_gid;
    int signo;

    if (!pmp) return EINVAL;

    memset(&msg, 0, sizeof(msg));
    msg.type = PM_VFS_EXEC;
    exec_req = (struct vfs_exec_request*)msg.MSG_PAYLOAD;
    exec_req->endpoint = ep;
    exec_req->pathname = m->PATHNAME;
    exec_req->name_len = m->NAME_LEN;
    exec_req->frame = m->BUF;
    exec_req->frame_size = m->BUF_LEN;

    send_recv(BOTH, TASK_FS, &msg);

    exec_resp = (struct vfs_exec_response*)msg.MSG_PAYLOAD;
    if (exec_resp->status) return exec_resp->status;

    pmp->frame_addr = exec_resp->frame;
    pmp->frame_size = exec_resp->frame_size;
    new_uid = exec_resp->new_uid;
    new_gid = exec_resp->new_gid;

    sugid = FALSE;

    /* Reset caught signals. */
    for (signo = 1; signo < NSIG; signo++) {
        if (sigismember(&pmp->sig_catch, signo)) {
            sigdelset(&pmp->sig_catch, signo);
            pmp->sigaction[signo].sa_handler = SIG_DFL;
            sigemptyset(&pmp->sigaction[signo].sa_mask);
        }
    }

    /* tell tracer */
    if (pmp->tracer != NO_TASK) {
        sig_proc(pmp, SIGTRAP, TRUE);
    } else {
        sugid = TRUE;
    }

    if (sugid) {
        if (pmp->effuid != new_uid) {
            pmp->effuid = new_uid;
            /* TODO: tell vfs */
        }

        if (pmp->effgid != new_gid) {
            pmp->effgid = new_gid;
            /* TODO: tell vfs */
        }
    }

    return 0;
}
