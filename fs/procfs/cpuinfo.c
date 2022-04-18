/*
    (c)Copyright 2011 Jimx

    This file is part of Lyos.

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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include "type.h"
#include "proto.h"
#include <asm/const.h>
#include <asm/cpu_info.h>

#if defined(__i386__) || defined(__x86_64__)
static const char* x86_flag[] = {
    "sse3",    "pclmulqdq",  "dtes64",  "monitor", "ds_cpl",
    "vmx",     "smx",        "est",     "tm2",     "ssse3",
    "cnxt_id", "sdbg",       "fma",     "cx16",    "xtpr",
    "pdcm",    "",           "pcid",    "dca",     "sse4_1",
    "sse4_2",  "x2apic",     "movbe",   "popcnt",  "tsc_deadline",
    "aes",     "xsave",      "osxsave", "avx",     "f16c",
    "rdrnd",   "hypervisor", "fpu",     "vme",     "de",
    "pse",     "tsc",        "msr",     "pae",     "mce",
    "cx8",     "apic",       "",        "sep",     "mtrr",
    "pge",     "mca",        "cmov",    "pat",     "pse36",
    "psn",     "clfsh",      "",        "ds",      "acpi",
    "mmx",     "fxsr",       "sse",     "sse2",    "ss",
    "htt",     "tm",         "ia64",    "pbe",
};

static void print_x86_cpu_flags(u32* flags)
{
    int i, j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 32; j++) {
            if (flags[i] & (1 << j) && x86_flag[i * 32 + j][0])
                buf_printf("%s ", x86_flag[i * 32 + j]);
        }
    }
    buf_printf("\n");
}
#endif

static void print_cpu(struct cpu_info* cpu_info, int idx)
{
    buf_printf("%-16s: %d\n", "processor", idx);

#if defined(__i386__) || defined(__x86_64__)
    switch (cpu_info->vendor) {
    case CPU_VENDOR_INTEL:
        buf_printf("%-16s: %s\n", "vendor_id", "GenuineIntel");
        break;
    case CPU_VENDOR_AMD:
        buf_printf("%-16s: %s\n", "vendor_id", "AuthenticAMD");
        break;
    default:
        buf_printf("%-16s: %s\n", "vendor_id", "Unknown");
        break;
    }

    buf_printf("%-16s: %d\n", "cpu family", cpu_info->family);
    buf_printf("%-16s: %d\n", "model", cpu_info->model);
    buf_printf("%-16s: %d\n", "stepping", cpu_info->stepping);
    buf_printf("%-16s: %d\n", "cpu MHz", cpu_info->freq_mhz);
    buf_printf("%-16s: %d\n", "apicid", cpu_info->apicid);
    buf_printf("%-16s: ", "flags");
    print_x86_cpu_flags(cpu_info->flags);
#elif defined(__riscv)
    buf_printf("%-16s: %lu\n", "hart", cpu_info->hart);
    buf_printf("%-16s: %s\n", "isa", cpu_info->isa);
    buf_printf("%-16s: %s\n", "mmu", cpu_info->mmu);
#endif
    buf_printf("\n");
}

void root_cpuinfo(void)
{
    struct machine machine;
    struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

    if (get_machine(&machine)) {
        printl("procfs: can't get machine info\n");
        return;
    }

    if (get_cpuinfo(cpu_info)) {
        printl("procfs: can't get cpu info\n");
        return;
    }

    int i;
    for (i = 0; i < machine.cpu_count; i++) {
        print_cpu(&cpu_info[i], i);
    }
}
