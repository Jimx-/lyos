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
#include "const.h"

PRIVATE void pm_init();

PUBLIC int main(int argc, char * argv[])
{
	pm_init();
    
    MESSAGE msg;

    while (TRUE) {
        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case FORK:
            msg.RETVAL = do_fork(&msg);
            break;
        case WAIT:
            msg.RETVAL = do_wait(&msg);
            break;
        case EXIT:
            msg.RETVAL = do_exit(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        if (msg.RETVAL != SUSPEND) {
            msg.type = SYSCALL_RET;
            send_recv(SEND, src, &msg);
        }
    }

    return 0;
}

PRIVATE void pm_init()
{
    int retval = 0;
    struct boot_proc boot_procs[NR_BOOT_PROCS];
    struct pmproc * pmp;
    MESSAGE vfs_msg;

	printl("PM: process manager is running.\n");

    if ((retval = get_bootprocs(boot_procs)) != 0) panic("cannot get boot procs from kernel");
    procs_in_use = 0;

    struct boot_proc * bp;
    for (bp = boot_procs; bp < &boot_procs[NR_BOOT_PROCS]; bp++) {
        if (bp->proc_nr < 0) continue;
        
        procs_in_use++;

        pmp = &pmproc_table[bp->proc_nr];
        pmp->flags = 0;
        if (bp->proc_nr == INIT) {
            pmp->parent = INIT;
            pmp->pid = INIT_PID;
        } else {
            if (bp->proc_nr == TASK_SERVMAN)
                pmp->parent = INIT;
            else pmp->parent = TASK_SERVMAN;

            pmp->pid = find_free_pid();
        }

        pmp->flags |= PMPF_INUSE;
        pmp->endpoint = bp->endpoint;

        /*
        vfs_msg.type = PM_VFS_INIT;
        vfs_msg.PROC_NR = bp->proc_nr;
        vfs_msg.PID = bp->pid;
        vfs_msg.ENDPOINT = bp->endpoint;
        send_recv(BOTH, TASK_FS, &vfs_msg); */
    }
}
