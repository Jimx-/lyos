/*  
    (c)Copyright 2014 Jimx
    
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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include "libsysfs/libsysfs.h"
#include "proto.h"
#include "global.h"

PRIVATE void servman_init();

PUBLIC int main()
{
    servman_init();

    while (TRUE) {
        MESSAGE msg;

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        int msgtype = msg.type;

        switch (msgtype) {
        case SERVICE_UP:
            msg.RETVAL = do_service_up(&msg);
            break;
        case SERVICE_INIT_REPLY:
            msg.RETVAL = do_service_init_reply(&msg);
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

PRIVATE void servman_init()
{
    printl("SERVMAN: service manager is running.\n");

    int i;
    /* prepare priv structure for all priv procs */
    for (i = 0; boot_priv_table[i].endpoint != NO_TASK; i++) {
        struct boot_priv * bpriv = &boot_priv_table[i];
        struct sproc * sp = &sproc_table[i];

        sp->flags |= SPF_INUSE;
        sp->priv.id = static_priv_id(ENDPOINT_P(bpriv->endpoint));
        sp->priv.flags = bpriv->flags;

        /* syscall mask */
        int j;
        if (sp->priv.flags == TASK_FLAGS) {
            for (j = 0; j < BITCHUNKS(NR_SYS_CALLS); j++) {
                sp->priv.allowed_syscalls[j] = ~0;
            } 
        } else if (sp->priv.flags == USER_FLAGS) {
            for (j = 0; j < BITCHUNKS(NR_SYS_CALLS); j++) {
                sp->priv.allowed_syscalls[j] = 0;
            } 
        }

        /* set privilege */
        if (bpriv->endpoint != TASK_MM && bpriv->endpoint != TASK_SERVMAN) {
            int r;
            if ((r = privctl(bpriv->endpoint, PRIVCTL_SET_PRIV, &(sp->priv))) != 0) panic("unable to set priv(%d)", r);
            if ((r = privctl(bpriv->endpoint, PRIVCTL_ALLOW, NULL)) != 0) panic("unable to allow priv(%d)", r);
        }

        sp->endpoint = bpriv->endpoint;
    }

    sysfs_publish_domain("services", SF_PRIV_OVERWRITE);
}
