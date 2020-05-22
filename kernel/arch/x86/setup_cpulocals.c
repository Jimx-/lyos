#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <asm/protect.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/interrupt.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <lyos/cpulocals.h>

DEFINE_CPULOCAL(unsigned int, cpu_number);

#define BOOT_CPULOCALS_OFFSET 0
vir_bytes __cpulocals_offset[CONFIG_SMP_MAX_CPUS] = {
    [0 ... CONFIG_SMP_MAX_CPUS - 1] = BOOT_CPULOCALS_OFFSET,
};

void arch_setup_cpulocals(void)
{
    vir_bytes size;
    char* ptr;
    int cpu;
    extern char _cpulocals_start[], _cpulocals_end[];

    size = roundup(_cpulocals_end - _cpulocals_start, ARCH_PG_SIZE);
    ptr = __va(pg_alloc_lowest(&kinfo, size * CONFIG_SMP_MAX_CPUS));

    for (cpu = 0; cpu < CONFIG_SMP_MAX_CPUS; cpu++) {
        cpulocals_offset(cpu) = ptr - (char*)_cpulocals_start;
        memcpy(ptr, (void*)_cpulocals_start, _cpulocals_end - _cpulocals_start);

        get_cpu_var(cpu, cpu_number) = cpu;

        ptr += size;
    }
}
