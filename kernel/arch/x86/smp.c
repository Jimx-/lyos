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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "acpi.h"
#include "apic.h"
#include "arch_const.h"
#include "arch_proto.h"
#include "arch_smp.h"

PRIVATE u8 apicid2cpuid[255];
PUBLIC u8 cpuid2apicid[CONFIG_SMP_MAX_CPUS];

PRIVATE u32 bsp_cpu_id, bsp_lapic_id;

PRIVATE int discover_cpus();
PRIVATE void init_tss_all();
PRIVATE void smp_start_aps();

PUBLIC void smp_init()
{
    if (!discover_cpus()) {
        ncpus = 1;
    }

    init_tss_all();

    lapic_addr = LOCAL_APIC_DEF_ADDR;

    bsp_lapic_id = apicid();
    bsp_cpu_id = apicid2cpuid[bsp_lapic_id];

    switch_k_stack((char *)get_k_stack_top(bsp_cpu_id) -
            X86_STACK_TOP_RESERVED, smp_start_aps);
}

PRIVATE void init_tss_all()
{
    unsigned cpu;


    for(cpu = 0; cpu < ncpus ; cpu++) {
        init_tss(cpu, (u32)get_k_stack_top(cpu)); 
    }
}

PRIVATE int discover_cpus()
{
    struct acpi_madt_lapic * cpu;

    while (ncpus < CONFIG_SMP_MAX_CPUS && (cpu = acpi_get_lapic_next())) {
        apicid2cpuid[cpu->apic_id] = ncpus;
        cpuid2apicid[ncpus] = cpu->apic_id;
        printk("CPU %3d local APIC id 0x%03x %s\n", ncpus, cpu->apic_id, ncpus == 0 ? "(bsp)" : "");
        ncpus++;
    }

    return ncpus;
}

PRIVATE void smp_start_aps()
{
    finish_bsp_booting();
}
