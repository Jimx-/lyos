#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "acpi.h"
#include "apic.h"
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/hwint.h>
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/spinlock.h>
#include <lyos/watchdog.h>
#include <lyos/time.h>
#include <asm/hpet.h>
#include <asm/div64.h>
#include <asm/type.h>

#define CPUID_CC 1 /* Core cycle event not available */

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];
void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

static struct arch_watchdog intel_watchdog;

int arch_watchdog_init(void)
{
    u32 eax, ebx, ecx, edx;
    u32 cpu = cpuid;

    if (cpu_info[cpu].vendor == CPU_VENDOR_INTEL) {
        eax = 0xa;
        _cpuid(&eax, &ebx, &ecx, &edx);

        if (ebx & (1 << CPUID_CC)) {
            return -1;
        }

        if (((eax >> 8) & 0xff) == 0) {
            return -1;
        }

        watchdog = &intel_watchdog;
    } else {
        return -1;
    }

    lapic_write(LAPIC_LVTPCR, APIC_ICR_INT_MASK | APIC_ICR_DM_NMI);
    (void)lapic_read(LAPIC_LVTPCR);

    if (lapic_addr && watchdog->init) {
        watchdog->init(cpuid);
    }

    return 0;
}

void arch_watchdog_stop(void) {}

static void intel_watchdog_init(unsigned int cpu)
{
    u64 cpu_freq;
    u32 val;
    ia32_write_msr(INTEL_MSR_PMC0, 0, 0);

    val = INTEL_MSR_ERFEVTSEL_INT_EN | INTEL_MSR_ERFEVTSEL_OS |
          INTEL_MSR_ERFEVTSEL_USR | INTEL_MSR_ERFEVTSEL_UNHALTED_CORE_CYCLES;
    ia32_write_msr(INTEL_MSR_ERFEVTSEL0, 0, val);

    cpu_freq = cpu_hz[cpu];
    while (ex64hi(cpu_freq) || ex64lo(cpu_freq) > 0x7fffffff)
        cpu_freq >>= 1;
    cpu_freq = make64(ex64hi(cpu_freq), -ex64lo(cpu_freq));

    watchdog->reset_val = watchdog->watchdog_reset_val = cpu_freq;

    ia32_write_msr(INTEL_MSR_PMC0, 0, ex64lo(watchdog->reset_val));
    ia32_write_msr(INTEL_MSR_ERFEVTSEL0, 0, val | INTEL_MSR_ERFEVTSEL_EN);

    /* unmask interrupt and deliver NMIs */
    lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
}

static void intel_watchdog_reset(unsigned int cpu)
{
    lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
    ia32_write_msr(INTEL_MSR_PMC0, 0, ex64lo(watchdog->reset_val));
}

static int intel_watchdog_init_profile(unsigned int freq)
{
    u64 cpu_freq;

    cpu_freq = cpu_hz[cpuid];
    do_div(cpu_freq, freq);

    cpu_freq = make64(0, -ex64lo(cpu_freq));
    watchdog->profile_reset_val = cpu_freq;

    return 0;
}

static struct arch_watchdog intel_watchdog = {
    .init = intel_watchdog_init,
    .reset = intel_watchdog_reset,
    .init_profile = intel_watchdog_init_profile,
};
