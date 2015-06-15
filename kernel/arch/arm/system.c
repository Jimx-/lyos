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
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include "libexec/libexec.h"
#include "arch_type.h"

PUBLIC struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

PUBLIC struct proc * arch_switch_to_user()
{
    struct proc * p;

    p = get_cpulocal_var(proc_ptr);

    return p;
}

PUBLIC void arch_boot_proc(struct proc * p, struct boot_proc * bp)
{

}

/*****************************************************************************
 *                                identify_cpu
 *****************************************************************************/
/**
 * <Ring 0> Identify a cpu.
 *
 *****************************************************************************/
PUBLIC void identify_cpu()
{

}

PUBLIC int arch_reset_proc(struct proc * p)
{
    return 0;
}

PUBLIC void arch_set_syscall_result(struct proc * p, int result)
{

}

PUBLIC int arch_init_proc(struct proc * p, void * sp, void * ip, struct ps_strings * ps, char * name)
{
    memcpy(p->name, name, PROC_NAME_LEN);
    
    return 0;
}
