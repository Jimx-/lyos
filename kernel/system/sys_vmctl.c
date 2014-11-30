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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include <lyos/cpulocals.h>
#include <lyos/vm.h>

/**
 * <Ring 0> Perform the VMCTL syscall.
 */
PUBLIC int sys_vmctl(MESSAGE * m, struct proc * p)
{
    int who = m->VMCTL_WHO, request = m->VMCTL_REQUEST;
    struct proc * target = (who == SELF) ? p : endpt_proc(who);

    switch (request) {
    case VMCTL_BOOTINHIBIT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_BOOTINHIBIT);
        break;
    case VMCTL_MMINHIBIT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_MMINHIBIT);
        break;
    case VMCTL_GET_KERN_MAPPING:
        m->VMCTL_GET_KM_RETVAL = arch_get_kern_mapping(m->VMCTL_GET_KM_INDEX, 
                                    (caddr_t*)&m->VMCTL_GET_KM_ADDR,
                                    &m->VMCTL_GET_KM_LEN,
                                    &m->VMCTL_GET_KM_FLAGS); 
        break;
    case VMCTL_REPLY_KERN_MAPPING:
        m->VMCTL_REPLY_KM_RETVAL = arch_reply_kern_mapping(m->VMCTL_REPLY_KM_INDEX,
                                    m->VMCTL_REPLY_KM_ADDR);
        break;
    case VMCTL_PAGEFAULT_CLEAR:
        if (!target) return EINVAL;
        PST_UNSET(target, PST_PAGEFAULT);
        break;
    }

    if (!target) return EINVAL;
    return arch_vmctl(m, target);
}
