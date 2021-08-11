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

#include <libfdt/libfdt.h>
#include <libof/libof.h>

static volatile int smp_commenced = 0;
static unsigned int cpu_nr;
static volatile int __cpu_ready;
void* __cpu_stack_pointer[CONFIG_SMP_MAX_CPUS];
void* __cpu_task_pointer[CONFIG_SMP_MAX_CPUS];

struct stackframe init_stackframe; /* used to retrieve the id of the init cpu */

static void smp_start_cpu(int hart_id, struct proc* idle_proc);

static int fdt_scan_hart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
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

unsigned int riscv_of_parent_hartid(const void* fdt, unsigned long offset)
{
    for (; offset >= 0; offset = fdt_parent_offset(fdt, offset)) {
        const char* compatible = fdt_getprop(fdt, offset, "compatible", NULL);
        if (!compatible || strcmp(compatible, "riscv") != 0) continue;

        const __be32* reg = fdt_getprop(fdt, offset, "reg", NULL);
        if (!reg) continue;

        u32 hart = be32_to_cpup(reg);
        return hart;
    }

    return -1;
}

static void smp_start_cpu(int hart_id, struct proc* idle_proc)
{
    unsigned int cpu = cpu_nr++;

    __cpu_ready = -1;
    cpu_to_hart_id[cpu] = hart_id;
    hart_to_cpu_id[hart_id] = cpu;

    idle_proc->regs.cpu = cpu;
    init_tss(hart_id, get_k_stack_top(cpu));

    __asm__ __volatile__("fence rw, rw" : : : "memory");

    __cpu_stack_pointer[hart_id] = get_k_stack_top(cpu);
    __cpu_task_pointer[hart_id] = (void*)idle_proc;

    while (__cpu_ready != cpu)
        ;

    set_cpu_flag(cpu, CPU_IS_READY);
}

void smp_init()
{
    bsp_cpu_id = 0;

    cpu_to_hart_id[bsp_cpu_id] = bsp_hart_id;
    hart_to_cpu_id[bsp_hart_id] = bsp_cpu_id;

    cpu_nr++;

    init_tss(cpuid, get_k_stack_top(cpuid));

    of_scan_fdt(fdt_scan_hart, NULL, initial_boot_params);

    finish_bsp_booting();
}

void smp_boot_ap()
{
    __cpu_ready = cpuid;

    printk("smp: CPU %d is up\n", cpuid);

    init_prot();

    get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);

    ap_finished_booting();

    while (!smp_commenced)
        ;

    switch_address_space(proc_addr(TASK_MM));

    plic_enable(cpuid);

    switch_to_user();
}

void smp_commence() { smp_commenced = 1; }
