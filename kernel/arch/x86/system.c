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

/**
 * <Ring 0> Switch back to user.
 */
PUBLIC struct proc * arch_switch_to_user()
{
    char * stack;
    struct proc * p;

#ifdef CONFIG_SMP
    stack = (char *)tss[cpuid].esp0;
#else
    stack = (char *)tss[0].esp0;
#endif

    p = get_cpulocal_var(proc_ptr);
    /* save the proc ptr on the stack */
    *((reg_t *)stack) = (reg_t)p;

    return p;
}

#define KM_SYSINFO  0
#define KM_GATE     1
#define KM_LAST     KM_GATE

extern char _sysinfo[], _esysinfo[], _gate[], _egate[];

/**
 * <Ring 0> Get kernel mapping information.
 */
PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    if (index > KM_LAST) return 1;
    
    if (index == KM_SYSINFO) {
        *addr = (caddr_t)((char *)*(&_sysinfo) - KERNEL_VMA);
        *len = (char *)*(&_esysinfo) - (char *)*(&_sysinfo);
        *flags = KMF_USER;
    }

    if (index == KM_GATE) {
        *addr = (caddr_t)((char *)*(&_gate) - KERNEL_VMA);
        *len = (char *)*(&_egate) - (char *)*(&_gate);
        *flags = KMF_USER;
    }

    return 0;
}

/**
 * <Ring 0> Set kernel mapping information.
 */
PUBLIC int arch_reply_kern_mapping(int index, void * vir_addr)
{
    char * sysinfo_start = (char *)*(&_sysinfo);
    char * gate_start = (char *)*(&_gate);
    char * start = NULL;

#define USER_PTR(x) (((char *)(x) - start) + (char *)vir_addr)

    if (index == KM_SYSINFO) {
        start = sysinfo_start;
        sysinfo.magic = SYSINFO_MAGIC;
        sysinfo_user = (struct sysinfo *)USER_PTR(&sysinfo);
        sysinfo.kinfo = (kinfo_t *)USER_PTR(&kinfo);
    } else if (index == KM_GATE) {
        start = gate_start;
        sysinfo.syscall_gate = (syscall_gate_t)USER_PTR(syscall_softint);
    }

    return 0;
}

/**
 * <Ring 0> Initialize FPU.
 */
PUBLIC void fpu_init()
{
    fninit();
}

