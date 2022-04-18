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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "errno.h"
#include "lyos/const.h"
#include <lyos/sysutils.h>
#include "proto.h"
#include "lyos/vm.h"
#include "global.h"

int do_procctl()
{
    endpoint_t who = mm_msg.PCTL_WHO;
    int param = mm_msg.PCTL_PARAM;
    int retval = 0;

    struct mmproc* mmp = endpt_mmproc(who);
    if (!mmp) return EINVAL;

    switch (param) {
    case PCTL_CLEARPROC: /* clear proc struct & mem regions */
        retval = proc_free(mmp, 1);
        break;
    case PCTL_CLEARMEM: /* clear mem regions only */
        if (!list_empty(&mmp->mm->mem_regions)) retval = proc_free(mmp, 0);
        break;
    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

int do_mm_getinfo()
{
    int request = mm_msg.REQUEST;
    endpoint_t src = mm_msg.source;
    size_t len = mm_msg.BUF_LEN;
    void* addr = mm_msg.BUF;

    switch (request) {
    case MM_GET_MEMINFO:
        if (len < sizeof(mem_info)) return EINVAL;

        return data_copy(src, addr, SELF, &mem_info, sizeof(mem_info));
    default:
        return EINVAL;
    }

    return EINVAL;
}
