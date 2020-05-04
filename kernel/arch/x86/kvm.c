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
