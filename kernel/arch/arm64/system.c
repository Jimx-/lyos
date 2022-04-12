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
#include "stddef.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/fs.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "asm/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include "libexec/libexec.h"
#include <asm/cpu_info.h>

struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

struct proc* arch_switch_to_user()
{
    struct proc* p;

    p = get_cpulocal_var(proc_ptr);

    return p;
}

void identify_cpu() {}

void arch_boot_proc(struct proc* p, struct boot_proc* bp) {}

int arch_reset_proc(struct proc* p) {}

void arch_set_syscall_result(struct proc* p, int result) {}

int arch_init_proc(struct proc* p, void* sp, void* ip, struct ps_strings* ps,
                   char* name)
{}

int arch_fork_proc(struct proc* p, struct proc* parent, int flags, void* newsp,
                   void* tls)
{}

void idle_stop() {}

void arch_init_profile_clock() {}

void arch_stop_profile_clock() {}

void arch_ack_profile_clock() {}
