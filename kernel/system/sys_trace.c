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

#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <signal.h>
#include <errno.h>
#include <lyos/trace.h>

int sys_trace(MESSAGE* m, struct proc* p_proc)
{
    struct proc* target = endpt_proc(m->TRACE_ENDPOINT);
    if (!target) return ESRCH;
    off_t offset = (off_t)m->TRACE_ADDR;
    void* addr = m->TRACE_ADDR;
    long data;
    int retval;

    struct vir_addr src, dest;

    switch (m->TRACE_REQ) {
    case TRACE_STOP:
        PST_SET(target, PST_TRACED);
        target->flags &= ~PF_TRACE_SYSCALL;
        return 0;
    case TRACE_SYSCALL:
        target->flags |= PF_TRACE_SYSCALL;
        PST_UNSET(target, PST_TRACED);
        return 0;
    case TRACE_CONT:
        PST_UNSET(target, PST_TRACED);
        return 0;
    case TRACE_PEEKTEXT:
    case TRACE_PEEKDATA: /* we don't separate text and data segments, so these
                             two are equivalent */
        src.addr = addr;
        src.proc_ep = target->endpoint;
        dest.addr = &data;
        dest.proc_ep = KERNEL;

        retval = vir_copy_check(p_proc, &dest, &src, sizeof(data));
        if (retval != 0) return EFAULT;

        m->TRACE_RET = data;
        break;
    case TRACE_PEEKUSER:
        if ((offset & (sizeof(long) - 1)) != 0) return EFAULT;
        if (offset <= sizeof(struct proc) - sizeof(long)) {
            m->TRACE_RET = *(long*)((char*)target + offset);
        }
        break;
    default:
        return EINVAL;
    }

    return 0;
}
