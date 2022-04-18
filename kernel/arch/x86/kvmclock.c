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
#include <sys/types.h>
#include <lyos/const.h>
#include <kernel/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/kvm_para.h>
#include <asm/pvclock.h>
#include <lyos/clocksource.h>
#include <asm/page.h>
#include <asm/proto.h>

static int msr_kvm_system_time;
static int msr_kvm_wall_clock;

static struct pvclock_vsyscall_time_info hvclock_info[CONFIG_SMP_MAX_CPUS];

static inline struct pvclock_vsyscall_time_info* this_cpu_hvclock(void)
{
    return &hvclock_info[cpuid];
}

static inline struct pvclock_vcpu_time_info* this_cpu_pvti(void)
{
    return &hvclock_info[cpuid].pvti;
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline u64 pvclock_scale_delta(u64 delta, u32 mul_frac, int shift)
{
    u64 product;
#ifdef __i386__
    u32 tmp1, tmp2;
#else
    ulong tmp;
#endif

    if (shift < 0)
        delta >>= -shift;
    else
        delta <<= shift;

#ifdef __i386__
    __asm__("mul  %5       ; "
            "mov  %4,%%eax ; "
            "mov  %%edx,%4 ; "
            "mul  %5       ; "
            "xor  %5,%5    ; "
            "add  %4,%%eax ; "
            "adc  %5,%%edx ; "
            : "=A"(product), "=r"(tmp1), "=r"(tmp2)
            : "a"((u32)delta), "1"((u32)(delta >> 32)), "2"(mul_frac));
#elif defined(__x86_64__)
    __asm__("mulq %[mul_frac] ; shrd $32, %[hi], %[lo]"
            : [lo] "=a"(product), [hi] "=d"(tmp)
            : "0"(delta), [mul_frac] "rm"((u64)mul_frac));
#else
#error implement me!
#endif

    return product;
}

static __attribute__((always_inline)) inline u64
pvclock_read_cycles(const struct pvclock_vcpu_time_info* src, u64 tsc)
{
    u64 delta = tsc - src->tsc_timestamp;
    u64 offset =
        pvclock_scale_delta(delta, src->tsc_to_system_mul, src->tsc_shift);
    return src->system_time + offset;
}

static u64 kvm_clock_read(struct clocksource* cs)
{
    unsigned int version;
    u64 tsc;
    u64 ret;
    struct pvclock_vcpu_time_info* src = this_cpu_pvti();

    do {
        version = src->version & ~1;
        cmb();
        read_tsc_64(&tsc);
        ret = pvclock_read_cycles(src, tsc);
        cmb();
    } while (version != src->version);

    return ret;
}

static struct clocksource kvmclock_clocksource = {
    .name = "kvm-clock",
    .rating = 400,
    .read = kvm_clock_read,
    .mask = 0xffffffffffffffff,
};

void kvm_register_clock(void)
{
    struct pvclock_vsyscall_time_info* src = this_cpu_hvclock();
    u64 pa;

    if (msr_kvm_system_time == 0) {
        return;
    }

    pa = (u64)__pa(&src->pvti) | 0x1ULL;
    ia32_write_msr(msr_kvm_system_time, ex64hi(pa), ex64lo(pa));

    printk("kvm-clock: cpu %d, msr %llx\n", cpuid, pa);
}

void init_kvmclock(void)
{
    if (!kvm_para_available()) {
        return;
    }

    if (kvm_para_has_feature(KVM_FEATURE_CLOCKSOURCE2)) {
        msr_kvm_system_time = MSR_KVM_SYSTEM_TIME_NEW;
        msr_kvm_wall_clock = MSR_KVM_WALL_CLOCK_NEW;
    } else if (kvm_para_has_feature(KVM_FEATURE_CLOCKSOURCE)) {
        msr_kvm_system_time = MSR_KVM_SYSTEM_TIME;
        msr_kvm_wall_clock = MSR_KVM_WALL_CLOCK;
    } else {
        msr_kvm_system_time = 0;
        msr_kvm_wall_clock = 0;
        return;
    }

    printk("kvm-clock: using msrs 0x%x and 0x%x\n", msr_kvm_system_time,
           msr_kvm_wall_clock);

    register_clocksource(&kvmclock_clocksource);
}
