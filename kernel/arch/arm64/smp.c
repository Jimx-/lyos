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
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/cpulocals.h>
#include <lyos/smp.h>
#include <asm/cpu_info.h>
#include <asm/pagetable.h>
#include <asm/cpu_type.h>
#include <asm/fixmap.h>
#include <asm/cache.h>
#include <asm/mach.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

static volatile int smp_commenced = 0;
static unsigned int cpu_nr;
static volatile int __cpu_ready;
void* __cpu_stack_pointer;
void* __cpu_task_pointer;

struct stackframe init_stackframe; /* used to retrieve the id of the init cpu */

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

static u64 __cpu_logical_map[CONFIG_SMP_MAX_CPUS] = {
    [0 ... CONFIG_SMP_MAX_CPUS - 1] = INVALID_HWID};

extern void secondary_entry_spin_table();

static unsigned int smp_start_cpu_spin_table(u64 hwid,
                                             phys_bytes cpu_release_addr);

static int fdt_scan_hart(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
{
    unsigned int cpu;
    u64 hwid;
    u64 cpu_release_addr;
    const u32* prop;

    const char* type = fdt_getprop(blob, offset, "device_type", NULL);
    if (!type || strcmp(type, "cpu") != 0) return 0;

    const u32* reg = fdt_getprop(blob, offset, "reg", NULL);
    if (!reg) return 0;

    hwid = be32_to_cpup(reg);
    if (hwid & ~MPIDR_HWID_BITMASK) return 0;
    if (hwid >= CONFIG_SMP_MAX_CPUS) return 0;

    if (hwid == __cpu_logical_map[bsp_cpu_id]) return 0;

    prop = fdt_getprop(blob, offset, "enable-method", NULL);
    if (!prop || strcmp((char*)prop, "spin-table")) return 0;

    prop = fdt_getprop(blob, offset, "cpu-release-addr", NULL);
    if (!prop) return 0;

    cpu_release_addr = of_read_number(prop, 2);

    cpu = smp_start_cpu_spin_table(hwid, cpu_release_addr);

    cpu_info[cpu].hwid = hwid;

    return 0;
}

static unsigned int smp_start_cpu_spin_table(u64 hwid,
                                             phys_bytes cpu_release_addr)
{
    unsigned int cpu = cpu_nr++;
    struct proc* idle_task = get_cpu_var_ptr(cpu, idle_proc);
    phys_bytes pa_entry = __pa_symbol(secondary_entry_spin_table);
    void* release_addr =
        (void*)set_fixmap_offset(FIX_CPU_RELEASE_ADDR, cpu_release_addr);

    __cpu_ready = -1;
    __cpu_logical_map[cpu] = hwid;

    idle_task->regs.cpu = cpu;
    init_tss(cpu, get_k_stack_top(cpu));

    __cpu_task_pointer = (void*)idle_task;
    __cpu_stack_pointer = get_k_stack_top(cpu);

    *(volatile unsigned long*)release_addr = pa_entry;
    dcache_clean_inval_poc((unsigned long)release_addr,
                           (unsigned long)release_addr + sizeof(unsigned long));

    sev();

    clear_fixmap(FIX_CPU_RELEASE_ADDR);

    while (__cpu_ready != cpu)
        arch_pause();

    set_cpu_flag(cpu, CPU_IS_READY);

    return cpu;
}

static void smp_setup_processor_id(void)
{
    u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;
    __cpu_logical_map[bsp_cpu_id] = mpidr;
}

void smp_init()
{
    bsp_cpu_id = 0;

    smp_setup_processor_id();
    set_my_cpu_offset(cpulocals_offset(bsp_cpu_id));
    cpu_nr++;

    init_tss(bsp_cpu_id, get_k_stack_top(bsp_cpu_id));

    of_scan_fdt(fdt_scan_hart, NULL, initial_boot_params);

    machine.cpu_count = cpu_nr;

    finish_bsp_booting();
}

void smp_boot_ap()
{
    __cpu_ready = cpuid;

    printk("smp: CPU %d is up\n", cpuid);

    identify_cpu();

    get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);

    ap_finished_booting();

    while (!smp_commenced)
        arch_pause();

    switch_address_space(proc_addr(TASK_MM));
    write_sysreg(__pa(swapper_pg_dir), ttbr1_el1);
    isb();

    if (machine_desc->init_cpu) machine_desc->init_cpu(cpuid);

    switch_to_user();
}

void smp_commence() { smp_commenced = 1; }
