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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <lyos/cpulocals.h>
#include <lyos/smp.h>

#include <libof/libof.h>

PRIVATE int smp_commenced = 0;
PRIVATE int __cpu_ready;
PUBLIC void* __cpu_stack_pointer[CONFIG_SMP_MAX_CPUS];
PUBLIC void* __cpu_task_pointer[CONFIG_SMP_MAX_CPUS];

PUBLIC struct stackframe init_stackframe;   /* used to retrieve the id of the init cpu */

PRIVATE void smp_start_cpu(int hart_id, struct proc* idle_proc);

PRIVATE int fdt_scan_hart(void* blob, unsigned long offset, const char* name, int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "device_type", NULL);
    if (!type || strcmp(type, "cpu") != 0) return 0;

    const u32* reg = fdt_getprop(blob, offset, "reg", NULL);
    if (!reg) return 0;

    u32 hart_id = be32_to_cpup(reg);
    if (hart_id >= CONFIG_SMP_MAX_CPUS) return 0;

    if (hart_id == cpuid) return 0;

    smp_start_cpu(hart_id, get_cpu_var_ptr(hart_id, idle_proc));

    return 0;
}

PRIVATE void smp_start_cpu(int hart_id, struct proc* idle_proc)
{
    __cpu_ready = -1;
    idle_proc->regs.cpu = hart_id;
    init_tss(hart_id, get_k_stack_top(hart_id));

    __asm__ __volatile__ ("fence rw, rw" : : : "memory");

    __cpu_stack_pointer[hart_id] = get_k_stack_top(hart_id);
    __cpu_task_pointer[hart_id] = (void*) idle_proc;

    while (__cpu_ready != hart_id) ;

    set_cpu_flag(hart_id, CPU_IS_READY);
}

PUBLIC void smp_init()
{
    init_tss(cpuid, get_k_stack_top(cpuid));

    of_scan_fdt(fdt_scan_hart, NULL, initial_boot_params);

    finish_bsp_booting();
}

PUBLIC void smp_boot_ap()
{
    __cpu_ready = cpuid;
    printk("smp: CPU %d is up\n", cpuid);

    init_prot();

    get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);

    ap_finished_booting();

    while (!smp_commenced) ;

    switch_address_space(proc_addr(TASK_MM));

    switch_to_user();
}

PUBLIC void smp_commence()
{
    smp_commenced = 1;
}
