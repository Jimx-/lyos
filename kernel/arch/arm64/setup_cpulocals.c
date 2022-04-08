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

DEFINE_CPULOCAL(unsigned int, cpu_number) = 0;

#define BOOT_CPULOCALS_OFFSET 0
vir_bytes __cpulocals_offset[CONFIG_SMP_MAX_CPUS] = {
    [0 ... CONFIG_SMP_MAX_CPUS - 1] = BOOT_CPULOCALS_OFFSET,
};

void arch_setup_cpulocals(void) {}
