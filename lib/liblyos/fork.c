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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include <string.h>
#include "lyos/vm.h"
#include <lyos/config.h>
#include <lyos/param.h>
#include <lyos/sysutils.h>

int kernel_fork(endpoint_t parent_ep, int child_proc, endpoint_t* child_ep,
                int flags, void* newsp, void* tls)
{
    MESSAGE m;
    struct kfork_info* kfi = (struct kfork_info*)m.MSG_PAYLOAD;

    memset(&m, 0, sizeof(m));
    kfi->parent = parent_ep;
    kfi->slot = child_proc;
    kfi->flags = flags;
    kfi->newsp = newsp;
    kfi->tls = tls;

    int retval = syscall_entry(NR_FORK, &m);
    if (retval) return retval;

    *child_ep = kfi->child;

    return 0;
}
