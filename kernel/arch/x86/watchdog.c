#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/const.h"
#include <errno.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include "apic.h"
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/watchdog.h>
#include <asm/div64.h>
#include <asm/cpu_info.h>

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

    cpu_freq = cpu_hz[cpu];
    while (ex64hi(cpu_freq) || ex64lo(cpu_freq) > 0x7fffffff)
        cpu_freq >>= 1;

    watchdog->reset_val = watchdog->watchdog_reset_val = -cpu_freq;

    /* set initial ctr0 value */
    ia32_write_msr(INTEL_MSR_PERF_FIXED_CTR0, ex64hi(watchdog->reset_val),
                   ex64lo(watchdog->reset_val));

    /* enable ctr0 */
    ia32_write_msr(INTEL_MSR_PERF_GLOBAL_CTRL, 0x1, 0);

    /* count in OS & user mode, interrupt enabled */
    ia32_write_msr(INTEL_MSR_PERF_FIXED_CTRL, 0, 0xb);

    /* unmask interrupt and deliver NMIs */
    lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
}

static void intel_watchdog_reset(unsigned int cpu)
{
    ia32_write_msr(INTEL_MSR_PERF_FIXED_CTRL, 0, 0x0);

    /* reload ctr0 value */
    ia32_write_msr(INTEL_MSR_PERF_FIXED_CTR0, ex64hi(watchdog->reset_val),
                   ex64lo(watchdog->reset_val));

    /* force host to reprogram ctr0 */
    ia32_write_msr(INTEL_MSR_PERF_FIXED_CTRL, 0, 0xb);

    /* unmask interrupt and deliver NMIs */
    lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
}

static int intel_watchdog_init_profile(unsigned int freq)
{
    u64 cpu_freq;

    cpu_freq = cpu_hz[cpuid];
    do_div(cpu_freq, freq);

    if (ex64hi(cpu_freq) || ex64lo(cpu_freq) > 0x7fffffffU) {
        printk("nmi watchdog ticks exceed 31 bits\n");
        return EINVAL;
    }

    cpu_freq = -cpu_freq;
    watchdog->profile_reset_val = cpu_freq;

    return 0;
}

static struct arch_watchdog intel_watchdog = {
    .init = intel_watchdog_init,
    .reset = intel_watchdog_reset,
    .init_profile = intel_watchdog_init_profile,
};
