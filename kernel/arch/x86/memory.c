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
#include "stddef.h"
#include "protect.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>

extern int syscall_style;

#define KM_USERMAPPED  0
#define KM_LAST     KM_USERMAPPED

extern char _usermapped[], _eusermapped[];

/**
 * <Ring 0> Get kernel mapping information.
 */
PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    if (index > KM_LAST) return 1;
    
    if (index == KM_USERMAPPED) {
        *addr = (caddr_t)((char *)*(&_usermapped) - KERNEL_VMA);
        *len = (char *)*(&_eusermapped) - (char *)*(&_usermapped);
        *flags = KMF_USER;
    }

    return 0;
}

/**
 * <Ring 0> Set kernel mapping information according to MM's parameter.
 */
PUBLIC int arch_reply_kern_mapping(int index, void * vir_addr)
{
    char * usermapped_start = (char *)*(&_usermapped);

#define USER_PTR(x) (((char *)(x) - usermapped_start) + (char *)vir_addr)

    if (index == KM_USERMAPPED) {
        sysinfo.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo *)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t *)USER_PTR(&kinfo);
        sysinfo.kern_log = (struct kern_log *)USER_PTR(&kern_log);

        if (syscall_style & SST_INTEL_SYSENTER) {
            printk("kernel: selecting intel SYSENTER syscall style\n");
        }
        sysinfo.syscall_gate = (syscall_gate_t)USER_PTR(syscall_int);
    }

    return 0;
}

PRIVATE void setcr3(struct proc * p, void * cr3, void * cr3_v)
{
    p->seg.cr3_phys = (u32)cr3;
    p->seg.cr3_vir = (u32 *)cr3_v;

    PST_UNSET(p, PST_MMINHIBIT);
}

PUBLIC int arch_vmctl(MESSAGE * m, struct proc * p)
{
    int request = m->VMCTL_REQUEST;

    switch (request) {
    case VMCTL_GETPDBR:
        m->VMCTL_VALUE = p->seg.cr3_phys;
        return 0;
    case VMCTL_SET_ADDRESS_SPACE:
        setcr3(p, m->VMCTL_PHYS_ADDR, m->VMCTL_VIR_ADDR);
        return 0;
    }

    return EINVAL;
}
