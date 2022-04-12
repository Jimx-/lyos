#ifndef _ARCH_CPU_TYPE_H_
#define _ARCH_CPU_TYPE_H_

#include <asm/sysreg.h>

#define INVALID_HWID ULONG_MAX

#define MPIDR_UP_BITMASK   (0x1 << 30)
#define MPIDR_MT_BITMASK   (0x1 << 24)
#define MPIDR_HWID_BITMASK UL(0xff00ffffff)

#define read_cpuid(reg) read_sysreg_s(SYS_##reg)

static inline u32 read_cpuid_id(void) { return read_cpuid(MIDR_EL1); }

static inline u64 read_cpuid_mpidr(void) { return read_cpuid(MPIDR_EL1); }

#endif
