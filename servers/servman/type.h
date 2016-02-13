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

#ifndef _SERVMAN_TYPE_H_
#define _SERVMAN_TYPE_H_

#include <lyos/priv.h>
#include <lyos/proc.h>
#include <lyos/service.h>
#include "const.h"

struct sproc {
    int flags;
	endpoint_t endpoint;
    pid_t pid;
	struct priv priv;	/* privilege structure */

    char cmdline[CMDLINE_MAX];
    char cmd_args[CMDLINE_MAX];
    char * argv[ARGV_MAX];
    int argc;

    char * exec;
    int exec_len;

    endpoint_t caller_e;
    int caller_request;

    /* ------------------- */
    char proc_name[PROC_NAME_LEN];
    char label[PROC_NAME_LEN];
    struct pci_acl pci_acl;
};

struct boot_priv {
	endpoint_t endpoint;
	char name[PROC_NAME_LEN];
	int flags;
};

#endif
