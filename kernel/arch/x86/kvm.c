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
#include <lyos/const.h>
#include <string.h>
#include <kernel/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/kvm_para.h>
#include <asm/bitops.h>
#include <asm/page.h>
#include <kernel/global.h>
#include <asm/proto.h>

#include "apic.h"

static int has_steal_clock = 0;

static DEFINE_CPULOCAL(struct kvm_steal_time, steal_time)
    __attribute__((aligned(64)));

static DEFINE_CPULOCAL(unsigned long, kvm_apic_eoi);

extern void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

static inline u32 _kvm_cpuid_base(void)
{
    u32 base, eax, ebx, ecx, edx;
    char signature[12];

    for (base = 0x40000000; base < 0x40010000; base += 0x100) {
        eax = base;
        _cpuid(&eax, &ebx, &ecx, &edx);

        memcpy(signature, &ebx, sizeof(ebx));
        memcpy(signature + 4, &ecx, sizeof(ecx));
        memcpy(signature + 8, &edx, sizeof(edx));

        if (!memcmp(signature, "KVMKVMKVM\0\0\0", 12)) {
            return base;
        }
    }

    return 0;
}

static inline int kvm_cpuid_base(void)
{
    static int base = -1;

    if (base == -1) {
        base = _kvm_cpuid_base();
    }

    return base;
}

int kvm_para_available(void) { return kvm_cpuid_base() != 0; }

unsigned int kvm_arch_para_features(void)
{
    u32 eax, ebx, ecx, edx;

    eax = kvm_cpuid_base() | KVM_CPUID_FEATURES;
    _cpuid(&eax, &ebx, &ecx, &edx);

    return eax;
}

static void kvm_register_steal_time(void)
{
    struct kvm_steal_time* st = get_cpulocal_var_ptr(steal_time);
    u64 val;

    if (!has_steal_clock) return;

    val = (u64)__pa(st) | KVM_MSR_ENABLED;
    ia32_write_msr(MSR_KVM_STEAL_TIME, ex64hi(val), ex64lo(val));

    printk("kvm-stealtime: cpu %d, msr %llx\n", cpuid,
           (unsigned long long)__pa(st));
}

static void kvm_guest_apic_eoi_write(void)
{
    if (arch_test_and_clear_bit_non_atomic(
            KVM_PV_EOI_BIT, get_cpulocal_var_ptr(kvm_apic_eoi))) {
        return;
    }

    apic->native_eoi_write();
}

void kvm_init_guest(void)
{
    if (kvm_para_has_feature(KVM_FEATURE_STEAL_TIME)) {
        has_steal_clock = 1;
    }

    if (kvm_para_has_feature(KVM_FEATURE_PV_EOI)) {
        apic_set_eoi_write(kvm_guest_apic_eoi_write);
    }
}

void kvm_init_guest_cpu(void)
{
    u64 val;

    if (has_steal_clock) kvm_register_steal_time();

    if (kvm_para_has_feature(KVM_FEATURE_PV_EOI)) {
        get_cpulocal_var(kvm_apic_eoi) = 0;
        val = (u64)__pa(get_cpulocal_var_ptr(kvm_apic_eoi)) | KVM_MSR_ENABLED;
        ia32_write_msr(MSR_KVM_PV_EOI_EN, ex64hi(val), ex64lo(val));
    }
}

u64 kvm_steal_clock(int cpu)
{
    u64 steal;
    struct kvm_steal_time* src;
    int version;

    if (!has_steal_clock) return 0;

    src = get_cpulocal_var_ptr(steal_time);
    do {
        version = src->version;
        cmb();
        steal = src->steal;
        cmb();
    } while ((version & 1) || (version != src->version));

    return steal;
}
