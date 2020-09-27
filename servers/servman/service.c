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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
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
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include "proto.h"
#include "const.h"
#include "type.h"
#include "global.h"
#include "libsysfs/libsysfs.h"

int create_service(struct sproc* sp);
int start_service(struct sproc* sp);
int run_service(struct sproc* sp, int init_type);
int init_service(struct sproc* sp, int init_type);
int publish_service(struct sproc* sp);

int check_permission(endpoint_t caller, int request)
{
    // struct proc * p = endpt_proc(caller);
    // if (p->euid != SU_UID) return EPERM;

    return 0;
}

int do_service_up(MESSAGE* msg)
{
    /* get parameters from the message */
    int src = msg->source; /* caller proc nr. */

    if (check_permission(src, SERVICE_UP) != 0) return EPERM;

    /* copy prog name */
    struct service_up_req up_req;
    data_copy(SELF, &up_req, src, msg->BUF, sizeof(up_req));

    struct sproc* psp;
    int retval = alloc_sproc(&psp);
    if (retval) return retval;

    retval = init_sproc(psp, &up_req, src);
    if (retval) return retval;

    retval = start_service(psp);
    if (retval) return retval;

    psp->flags |= SPF_LATEREPLY;
    psp->caller_e = src;
    psp->caller_request = SERVICE_UP;

    return SUSPEND;
}

int do_service_init_reply(MESSAGE* msg)
{
    int slot = ENDPOINT_P(msg->source);
    int result = msg->RETVAL;

    struct sproc* sp = sproc_ptr[slot];
    if (!sp) return SUSPEND; /* should not happen */

    if (!(sp->flags & SPF_INITIALIZING)) {
        return EINVAL;
    }

    if (result != 0) return SUSPEND;

    sp->flags &= ~SPF_INITIALIZING;

    late_reply(sp, 0);

    return SUSPEND;
}

int start_service(struct sproc* sp)
{
    int retval = create_service(sp);
    if (retval) return retval;

    retval = publish_service(sp);
    if (retval) return retval;

    retval = run_service(sp, SERVICE_INIT_FRESH);
    if (retval) return retval;

    return 0;
}

int create_service(struct sproc* sp)
{
    int child_pid = fork();

    if (child_pid < 0) {
        free_sproc(sp);
        return child_pid;
    }

    endpoint_t child_ep;
    int retval = get_procep(child_pid, &child_ep);
    if (retval) return retval;

    int child_slot = ENDPOINT_P(child_ep);
    sp->endpoint = child_ep;
    sproc_ptr[child_slot] = sp;
    sp->pid = child_pid;
    sp->flags |= SPF_INUSE;

    sp->priv.flags |= PRF_DYN_ID;
    if ((retval = privctl(sp->endpoint, PRIVCTL_SET_PRIV, &sp->priv)) != 0) {
        /* XXX: cleanup */
        return retval;
    }

    if ((retval = read_exec(sp)) != 0) return retval;

    if ((retval = serv_exec(sp->endpoint, sp->exec, sp->exec_len, sp->proc_name,
                            sp->argv)) != 0)
        return retval;
    free(sp->exec);
    sp->exec = NULL;

    return 0;
}

int run_service(struct sproc* sp, int init_type)
{
    int r;
    if ((r = privctl(sp->endpoint, PRIVCTL_ALLOW, NULL)) != 0) return r;

    if ((r = init_service(sp, init_type) != 0)) return r;

    return 0;
}

int init_service(struct sproc* sp, int init_type)
{
    MESSAGE m;
    m.type = SERVICE_INIT;
    m.REQUEST = init_type;

    send_recv(SEND, sp->endpoint, &m);
    sp->flags |= SPF_INITIALIZING;

    return 0;
}

int publish_service(struct sproc* sp)
{
    char label[MAX_PATH];
    int retval;

    char* name = sp->label;
    sprintf(label, SYSFS_SERVICE_DOMAIN_LABEL, name);
    retval = sysfs_publish_domain(label, SF_PRIV_OVERWRITE);

    sprintf(label, SYSFS_SERVICE_ENDPOINT_LABEL, name);
    retval = sysfs_publish_u32(label, sp->endpoint, SF_PRIV_OVERWRITE);

    sp->pci_acl.endpoint = sp->endpoint;
    if (sp->pci_acl.nr_pci_class > 0 || sp->pci_acl.nr_pci_id > 0) {
        pci_set_acl(&sp->pci_acl);
    }

    if (sp->nr_domain) {
        retval = mapdriver(sp->label, sp->domain, sp->nr_domain);
        if (retval) return retval;
    }

    return retval;
}
