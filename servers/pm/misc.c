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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "proto.h"
#include "const.h"
#include "global.h"

PUBLIC int do_getsetid(MESSAGE * p)
{
    int retval = 0;
    struct pmproc * pmp = pm_endpt_proc(p->source);
    if (!pmp) return EINVAL;
    uid_t uid;
    gid_t gid;
    int tell_vfs = 0;
    MESSAGE m;

    memset(&m, 0, sizeof(MESSAGE));

    switch (p->REQUEST) {
    case GS_GETEP:
        p->ENDPOINT = p->source;
        break;
    case GS_GETPID:
        p->PID = pmp->pid;
        break;
    case GS_GETUID:
        retval = pmp->realuid;
        break;
    case GS_GETGID:
        retval = pmp->realgid;
        break;
    case GS_GETEUID:
        retval = pmp->effuid;
        break;
    case GS_GETEGID:
        retval = pmp->effgid;
        break; 

    case GS_SETUID:
        uid = p->NEWID;

        if (uid != pmp->realuid && pmp->effuid != SU_UID) return EPERM;
        
        pmp->realuid = pmp->effuid = uid;

        m.type = PM_VFS_GETSETID;
        m.REQUEST = GS_SETUID;
        m.ENDPOINT = p->source;
        m.UID = pmp->realuid;
        m.EUID = pmp->effuid;

        tell_vfs = 1;
        break;
    case GS_SETGID:
        gid = p->NEWID;

        if (gid != pmp->realgid && pmp->effuid != SU_UID) return EPERM;
        
        pmp->realgid = pmp->effgid = gid;

        m.type = PM_VFS_GETSETID;
        m.REQUEST = GS_SETGID;
        m.ENDPOINT = p->source;
        m.GID = pmp->realgid;
        m.EGID = pmp->effgid;

        tell_vfs = 1;
        break;

    default:
        retval = EINVAL;
        break;
    }

    if (tell_vfs) {
        send_recv(SEND, TASK_FS, &m);
        return SUSPEND;
    }

    return retval;
}
