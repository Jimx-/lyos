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
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <lyos/cpulocals.h>
#include <lyos/kvm_para.h>
#include <asm/pvclock.h>

static int msr_kvm_system_time;
static int msr_kvm_wall_clock;

static struct pvclock_vsyscall_time_info hvclock_info[CONFIG_SMP_MAX_CPUS];

static inline struct pvclock_vsyscall_time_info* this_cpu_hvclock(void)
{
    return &hvclock_info[cpuid];
}

static void kvm_register_clock(void)
{
    struct pvclock_vsyscall_time_info* src = this_cpu_hvclock();
    u64 pa;

    pa = (u64)__pa(&src->pvti) | 0x1ULL;
    ia32_write_msr(msr_kvm_system_time, ex64hi(pa), ex64lo(pa));
}

void init_kvmclock()
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

    kvm_register_clock();
}
