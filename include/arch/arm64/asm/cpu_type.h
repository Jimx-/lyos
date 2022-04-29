#ifndef _ARCH_CPU_TYPE_H_
#define _ARCH_CPU_TYPE_H_

#include <asm/sysreg.h>

#define INVALID_HWID ULONG_MAX

#define MPIDR_UP_BITMASK   (0x1 << 30)
#define MPIDR_MT_BITMASK   (0x1 << 24)
#define MPIDR_HWID_BITMASK UL(0xff00ffffff)

#define MIDR_REVISION_MASK      0xf
#define MIDR_REVISION(midr)     ((midr)&MIDR_REVISION_MASK)
#define MIDR_PARTNUM_SHIFT      4
#define MIDR_PARTNUM_MASK       (0xfff << MIDR_PARTNUM_SHIFT)
#define MIDR_PARTNUM(midr)      (((midr)&MIDR_PARTNUM_MASK) >> MIDR_PARTNUM_SHIFT)
#define MIDR_ARCHITECTURE_SHIFT 16
#define MIDR_ARCHITECTURE_MASK  (0xf << MIDR_ARCHITECTURE_SHIFT)
#define MIDR_ARCHITECTURE(midr) \
    (((midr)&MIDR_ARCHITECTURE_MASK) >> MIDR_ARCHITECTURE_SHIFT)
#define MIDR_VARIANT_SHIFT     20
#define MIDR_VARIANT_MASK      (0xf << MIDR_VARIANT_SHIFT)
#define MIDR_VARIANT(midr)     (((midr)&MIDR_VARIANT_MASK) >> MIDR_VARIANT_SHIFT)
#define MIDR_IMPLEMENTOR_SHIFT 24
#define MIDR_IMPLEMENTOR_MASK  (0xff << MIDR_IMPLEMENTOR_SHIFT)
#define MIDR_IMPLEMENTOR(midr) \
    (((midr)&MIDR_IMPLEMENTOR_MASK) >> MIDR_IMPLEMENTOR_SHIFT)

#define read_cpuid(reg) read_sysreg_s(SYS_##reg)

static inline u32 read_cpuid_id(void) { return read_cpuid(MIDR_EL1); }

static inline u64 read_cpuid_mpidr(void) { return read_cpuid(MPIDR_EL1); }

#endif
