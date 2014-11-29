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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include "lyos/proc.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>
#include "global.h"
#include "proto.h"

/**
 * @brief Perform the FORK syscall.
 * 
 * @param p Ptr to message.
 * @return Zero on success, otherwise the error code.
 */
PUBLIC int do_fork(MESSAGE * p)
{
    int child_slot = 0, n = 0;
    endpoint_t parent_ep = p->source, child_ep;

    if (procs_in_use >= NR_PROCS + NR_TASKS) return EAGAIN; /* proc table full */

    do {
        child_slot = (child_slot + 1) % (NR_PROCS + NR_TASKS);
        n++;
    } while((pmproc_table[child_slot].flags & PMPF_INUSE) && n <= NR_PROCS);

    if (n > NR_PROCS) return EAGAIN;

    /* tell MM */
    MESSAGE msg2mm;
    msg2mm.type = PM_MM_FORK;
    msg2mm.ENDPOINT = parent_ep;
    msg2mm.PROC_NR = child_slot;
    send_recv(BOTH, TASK_MM, &msg2mm);
    if (msg2mm.RETVAL != 0) return msg2mm.RETVAL;
    child_ep = msg2mm.ENDPOINT;

    struct pmproc * pmp = &pmproc_table[child_slot];
    pmp->flags = PMPF_INUSE;

    p->PID = child_slot;

    /* tell FS, see fs_fork() */
    MESSAGE msg2fs;
    msg2fs.type = PM_VFS_FORK;
    msg2fs.PID = child_slot;
    send_recv(BOTH, TASK_FS, &msg2fs);

    /* birth of the child */
    MESSAGE msg2child;
    msg2child.type = SYSCALL_RET;
    msg2child.RETVAL = 0;
    msg2child.PID = 0;
    send_recv(SEND, child_ep, &msg2child);

    return 0;
}
