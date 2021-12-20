#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/cpufeature.h>
#include <stdint.h>
#include <string.h>

void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

static u32 cpuid_eax(u32 eax)
{
    u32 ebx, ecx, edx;

    ebx = ecx = edx = 0;
    _cpuid(&eax, &ebx, &ecx, &edx);

    return eax;
}

int _cpufeature(int cpufeature)
{
    u32 eax, ebx, ecx, edx;
    u32 ef_eax = 0, ef_ebx = 0, ef_ecx = 0, ef_edx = 0;
    unsigned int family, model, stepping;
    int is_intel = 0;
    u32 extended_cpuid_level;

    eax = ebx = ecx = edx = 0;

    /* We assume >= pentium for cpuid */
    eax = 0;
    _cpuid(&eax, &ebx, &ecx, &edx);
    if (eax > 0) {
        char vendor[12];
        memcpy(vendor, &ebx, sizeof(ebx));
        memcpy(vendor + 4, &edx, sizeof(edx));
        memcpy(vendor + 8, &ecx, sizeof(ecx));
        if (!strncmp(vendor, "GenuineIntel", sizeof(vendor))) is_intel = 1;
        eax = 1;
        _cpuid(&eax, &ebx, &ecx, &edx);
    } else
        return 0;

    stepping = eax & 0xf;
    model = (eax >> 4) & 0xf;

    if (model == 0xf || model == 0x6) {
        model += ((eax >> 16) & 0xf) << 4;
    }

    family = (eax >> 8) & 0xf;

    if (family == 0xf) {
        family += (eax >> 20) & 0xff;
    }

    extended_cpuid_level = cpuid_eax(0x80000000);
    if ((extended_cpuid_level & 0xffff0000) == 0x80000000) {
        if (extended_cpuid_level >= 0x80000001) {
            ef_eax = 0x80000001;
            _cpuid(&ef_eax, &ef_ebx, &ef_ecx, &ef_edx);
        }
    }

    switch (cpufeature) {
    case _CPUF_I386_PSE:
        return edx & CPUID1_EDX_PSE;
    case _CPUF_I386_PGE:
        return edx & CPUID1_EDX_PGE;
    case _CPUF_I386_APIC_ON_CHIP:
        return edx & CPUID1_EDX_APIC_ON_CHIP;
    case _CPUF_I386_TSC:
        return edx & CPUID1_EDX_TSC;
    case _CPUF_I386_FPU:
        return edx & CPUID1_EDX_FPU;
#define SSE_FULL_EDX (CPUID1_EDX_FXSR | CPUID1_EDX_SSE | CPUID1_EDX_SSE2)
#define SSE_FULL_ECX \
    (CPUID1_ECX_SSE3 | CPUID1_ECX_SSSE3 | CPUID1_ECX_SSE4_1 | CPUID1_ECX_SSE4_2)
    case _CPUF_I386_SSE1234_12:
        return (edx & SSE_FULL_EDX) == SSE_FULL_EDX &&
               (ecx & SSE_FULL_ECX) == SSE_FULL_ECX;
    case _CPUF_I386_FXSR:
        return edx & CPUID1_EDX_FXSR;
    case _CPUF_I386_SSE:
        return edx & CPUID1_EDX_SSE;
    case _CPUF_I386_SSE2:
        return edx & CPUID1_EDX_SSE2;
    case _CPUF_I386_SSE3:
        return ecx & CPUID1_ECX_SSE3;
    case _CPUF_I386_SSSE3:
        return ecx & CPUID1_ECX_SSSE3;
    case _CPUF_I386_SSE4_1:
        return ecx & CPUID1_ECX_SSE4_1;
    case _CPUF_I386_SSE4_2:
        return ecx & CPUID1_ECX_SSE4_2;
    case _CPUF_I386_HYPERVISOR:
        return ecx & CPUID1_ECX_HYPERVISOR;
    case _CPUF_I386_HAS_X2APIC:
        return ecx & CPUID1_ECX_X2APIC;
    case _CPUF_I386_HTT:
        return edx & CPUID1_EDX_HTT;
    case _CPUF_I386_HTT_MAX_NUM:
        return (ebx >> 16) & 0xff;
    case _CPUF_I386_SYSENTER:
        if (!is_intel) return 0;
        if (!(edx & CPUID1_EDX_SYSENTER)) return 0;
        if (family == 6 && model < 3 && stepping < 3) return 0;
        return 1;
    case _CPUF_I386_SYSCALL:
        return !!(ef_edx & CPUID_EF_EDX_SYSENTER);
    case _CPUF_I386_GBPAGES:
        return !!(ef_edx & CPUID_EF_EDX_PDPE1GB);
    }

    return 0;
}
