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
#include "lyos/tty.h"
#include "lyos/console.h"
#include "string.h"
#include "lyos/fs.h"
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
PUBLIC int sys_vmctl(int _unused1, int _unused2, MESSAGE * m, struct proc * p)
{
    int param = m->FLAGS, request = m->REQUEST;
    struct proc * target = proc_table + param;
    MESSAGE * msg = (MESSAGE *)param;

    switch (request) {
    case VMCTL_BOOTINHIBIT_CLEAR:
        PST_UNSET(target, PST_BOOTINHIBIT);
        break;
    case VMCTL_MMINHIBIT_CLEAR:
        PST_UNSET(target, PST_MMINHIBIT);
        break;
    case VMCTL_GET_KERN_MAPPING:
        msg->VMCTL_GET_KM_RETVAL = arch_get_kern_mapping(msg->VMCTL_GET_KM_INDEX, 
                                    (caddr_t*)&msg->VMCTL_GET_KM_ADDR,
                                    &msg->VMCTL_GET_KM_LEN,
                                    &msg->VMCTL_GET_KM_FLAGS); 
        break;
    case VMCTL_REPLY_KERN_MAPPING:
        msg->VMCTL_REPLY_KM_RETVAL = arch_reply_kern_mapping(msg->VMCTL_REPLY_KM_INDEX,
                                    msg->VMCTL_REPLY_KM_ADDR);
        break;
    default:
        printk("kernel: invalid vmctl request\n");
        return EINVAL;
    }

    return 0;
}
