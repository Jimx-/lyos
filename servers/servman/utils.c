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

#include <lyos/type.h>
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
#include <fcntl.h>
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <sys/stat.h>
#include "proto.h"
#include "const.h"
#include "global.h"

PUBLIC int init_sproc(struct sproc* sp, struct service_up_req* up_req,
                      endpoint_t source)
{
    sp->priv.flags = TASK_FLAGS;

    if (up_req->nr_pci_class > NR_PCI_CLASS) return EINVAL;
    if (up_req->nr_pci_class > 0) {
        sp->pci_acl.nr_pci_class = up_req->nr_pci_class;
        int i;
        for (i = 0; i < sp->pci_acl.nr_pci_class; i++) {
            sp->pci_acl.pci_class[i].classid = up_req->pci_class[i].classid;
            sp->pci_acl.pci_class[i].mask = up_req->pci_class[i].mask;
        }
    }

    if (up_req->nr_pci_id > NR_PCI_DEVICE) return EINVAL;
    if (up_req->nr_pci_id > 0) {
        sp->pci_acl.nr_pci_id = up_req->nr_pci_id;
        int i;
        for (i = 0; i < sp->pci_acl.nr_pci_id; i++) {
            sp->pci_acl.pci_id[i].vid = up_req->pci_id[i].vid;
            sp->pci_acl.pci_id[i].did = up_req->pci_id[i].did;
        }
    }

    sp->label[0] = '\0';
    set_sproc(sp, up_req, source);
    return 0;
}

PRIVATE void set_cmd_args(struct sproc* sp)
{
    strcpy(sp->cmd_args, sp->cmdline);
    char* p = sp->cmd_args;
    int argc = 0;

    sp->argv[argc++] = p;

    while (*p != '\0') {
        if (*p == ' ') {
            *p = '\0';
            while (*++p == ' ')
                ;
            if (*p == '\0') break;
            if (argc + 1 == ARGV_MAX) break;
            sp->argv[argc++] = p;
        }
        p++;
    }

    sp->argv[argc] = NULL;
    sp->argc = argc;
}

PUBLIC int set_sproc(struct sproc* sp, struct service_up_req* up_req,
                     endpoint_t source)
{
    if (up_req->cmdlen > CMDLINE_MAX) return E2BIG;
    data_copy(SELF, sp->cmdline, source, up_req->cmdline, up_req->cmdlen);
    sp->cmdline[up_req->cmdlen] = '\0';
    set_cmd_args(sp);

    if (up_req->prognamelen > PROC_NAME_LEN) return E2BIG;
    data_copy(SELF, sp->proc_name, source, up_req->progname,
              up_req->prognamelen);
    sp->proc_name[up_req->prognamelen] = '\0';

    if (!strcmp(sp->label, "")) {
        if (up_req->labellen) {
            if (up_req->labellen > PROC_NAME_LEN) return E2BIG;
            data_copy(SELF, sp->label, source, up_req->label, up_req->labellen);
            sp->label[up_req->labellen] = '\0';
        } else {
            int len = strlen(sp->proc_name);
            memcpy(sp->label, sp->proc_name, len);
            sp->label[len] = '\0';
        }
    }

    return 0;
}

PUBLIC int alloc_sproc(struct sproc** ppsp)
{
    int i;
    for (i = 0; i < NR_PRIV_PROCS; i++) {
        if (!(sproc_table[i].flags & SPF_INUSE)) {
            *ppsp = &sproc_table[i];
            return 0;
        }
    }
    return ENOMEM;
}

PUBLIC int free_sproc(struct sproc* sp)
{
    sp->flags &= ~SPF_INUSE;

    return 0;
}

PUBLIC int read_exec(struct sproc* sp)
{
    char* name = sp->argv[0];

    struct stat sbuf;

    int retval = stat(name, &sbuf);
    if (retval) return retval;

    int fd = open(name, O_RDONLY);
    if (fd == -1) return errno;

    sp->exec_len = sbuf.st_size;
    sp->exec = (char*)malloc(sp->exec_len);
    if (!sp->exec) {
        close(fd);
        return ENOMEM;
    }

    int n = read(fd, sp->exec, sp->exec_len);
    close(fd);

    if (n == sp->exec_len) return 0;

    free(sp->exec);

    if (n > 0)
        return EIO;
    else
        return errno;
}

PUBLIC int late_reply(struct sproc* sp, int retval)
{
    MESSAGE msg;

    if (!(sp->flags & SPF_LATEREPLY)) return EINVAL;

    memset(&msg, 0, sizeof(msg));
    msg.type = SYSCALL_RET;
    msg.RETVAL = retval;

    send_recv(SEND, sp->caller_e, &msg);
    sp->flags &= ~SPF_LATEREPLY;

    return 0;
}
