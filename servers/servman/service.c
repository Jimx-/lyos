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
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "errno.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "proto.h"
#include "const.h"
#include "libsysfs/libsysfs.h"

PUBLIC int check_permission(endpoint_t caller, int request)
{
    //struct proc * p = endpt_proc(caller);
    //if (p->euid != SU_UID) return EPERM;

    return 0;
}

PUBLIC int do_service_up(MESSAGE * msg)
{
    /* get parameters from the message */
    int name_len = msg->NAME_LEN; /* length of filename */
    int src = msg->source;    /* caller proc nr. */

    if (check_permission(src, SERVICE_UP) != 0) return EPERM;

    /* copy prog name */
    char pathname[MAX_PATH];
    data_copy(SELF, pathname, src, msg->PATHNAME, name_len);
    pathname[name_len] = 0; /* terminate the string */

    int child_pid = fork();
    if (child_pid) {
        /* block the child */
        //p = proc_table + child_pid;
        //p->state = PST_BOOTINHIBIT;    
    } else {
        while (1);
    }

    int retval = serv_exec(child_pid, pathname);
    if (retval) return retval;

    privctl(child_pid, PRIVCTL_SET_TASK, NULL);

    return 0;
}

PUBLIC int announce_service(char * name, endpoint_t serv_ep)
{
    char label[MAX_PATH];
    sprintf(label, SYSFS_SERVICE_ENDPOINT_LABEL, name);
    /* publish the endpoint */
    return sysfs_publish_u32(label, serv_ep, SF_PRIV_RETRIEVE);
}
