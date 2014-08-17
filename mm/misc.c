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
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "region.h"
#include "proto.h"
#include "const.h"
#include "lyos/vm.h"

PUBLIC int do_getsetid()
{
    int retval;
    int id = mm_msg.NEWID;

    switch (mm_msg.REQUEST) {
        case GS_GETUID:
            retval = current->uid;
            break;
        case GS_SETUID:
            retval = 0;
            current->uid = id;
            break;
        case GS_GETGID:
            retval = current->gid;
            break;
        case GS_SETGID:
            retval = 0;
            current->gid = id;
            break;
        case GS_GETEUID:
            retval = current->euid;
            break;
        case GS_GETEGID:
            retval = current->egid;
            break; 
    }

    return retval;
}

PUBLIC int do_procctl()
{
    endpoint_t who = mm_msg.PCTL_WHO;
    int param = mm_msg.PCTL_PARAM;
    int retval = 0;

    struct proc * p = proc_table + who;

    switch (param) {
        case PCTL_CLEARPROC:
            if (mm_msg.source != TASK_FS) retval = EPERM;
            else if (!list_empty(&(p->mem_regions))) retval = proc_free(p);
            break;
        default:
            retval = EINVAL;
            break;
    }

    return retval;
}
