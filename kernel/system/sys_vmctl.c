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
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include "signal.h"
#include <asm/page.h>
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/cpulocals.h>
#include <lyos/vm.h>

/**
 * <Ring 0> Perform the VMCTL syscall.
 */
int sys_vmctl(MESSAGE* m, struct proc* p)
{
    int who = m->VMCTL_WHO, request = m->VMCTL_REQUEST;
    struct proc* target = (who == SELF) ? p : endpt_proc(who);
    caddr_t addr;
    int len, flags;
    int type;

    switch (request) {
    case VMCTL_BOOTINHIBIT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_BOOTINHIBIT);
        return 0;
    case VMCTL_MMINHIBIT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_MMINHIBIT);
        return 0;
    case VMCTL_GET_KERN_MAPPING:
        m->VMCTL_GET_KM_RETVAL = arch_get_kern_mapping(
            m->VMCTL_GET_KM_INDEX, (caddr_t*)&addr, &len, &flags);
        m->VMCTL_GET_KM_ADDR = addr;
        m->VMCTL_GET_KM_LEN = len;
        m->VMCTL_GET_KM_FLAGS = flags;
        return 0;
    case VMCTL_REPLY_KERN_MAPPING:
        m->VMCTL_REPLY_KM_RETVAL = arch_reply_kern_mapping(
            m->VMCTL_REPLY_KM_INDEX, m->VMCTL_REPLY_KM_ADDR);
        return 0;
    case VMCTL_PAGEFAULT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_PAGEFAULT);
        return 0;
    case VMCTL_CLEAR_MEMCACHE:
        clear_memcache();
        return 0;
    case VMCTL_GET_MMREQ:
        if (!mmrequest) return ESRCH;

        switch (mmrequest->mm_request.req_type) {
        case MMREQ_CHECK:
            m->VMCTL_MMREQ_TARGET = mmrequest->mm_request.target;
            m->VMCTL_MMREQ_ADDR =
                (void*)mmrequest->mm_request.params.check.start;
            m->VMCTL_MMREQ_LEN = mmrequest->mm_request.params.check.len;
            m->VMCTL_MMREQ_FLAGS = mmrequest->mm_request.params.check.write;
            m->VMCTL_MMREQ_CALLER = mmrequest->endpoint;
            break;
        default:
            panic("wrong mm request type");
        }

        mmrequest->mm_request.result = MMSUSPEND;
        type = mmrequest->mm_request.req_type;
        mmrequest = mmrequest->mm_request.next_request;

        return type;
    case VMCTL_REPLY_MMREQ:
        if (!target) return EINVAL;

        target->mm_request.result = m->VMCTL_VALUE;

        switch (target->mm_request.type) {
        case MMREQ_TYPE_SYSCALL:
            target->flags |= PF_RESUME_SYSCALL;
            break;
        case MMREQ_TYPE_DELIVERMSG:
            break;
        default:
            panic("wrong mm request type: %d", target->mm_request.type);
            break;
        }

        PST_UNSET(target, PST_MMREQUEST);
        return 0;
    default:
        break;
    }

    if (!target) return EINVAL;
    return arch_vmctl(m, target);
}
