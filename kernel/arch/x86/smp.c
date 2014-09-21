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
#include "acpi.h"

PRIVATE u8 apicid2cpuid[255];
PUBLIC u8 cpuid2apicid[CONFIG_SMP_MAX_CPUS];

PRIVATE int discover_cpus();

PUBLIC void smp_init()
{
    if (!discover_cpus()) {
        ncpus = 1;
    }

}

PRIVATE int discover_cpus()
{
    struct acpi_madt_lapic * cpu;

    while (ncpus < CONFIG_SMP_MAX_CPUS && (cpu = acpi_get_lapic_next())) {
        apicid2cpuid[cpu->apic_id] = ncpus;
        cpuid2apicid[ncpus] = cpu->apic_id;
        disp_str("CPU %3d local APIC id 0x%03x\n", ncpus, cpu->apic_id);
        ncpus++;
    }

    return ncpus;
}
