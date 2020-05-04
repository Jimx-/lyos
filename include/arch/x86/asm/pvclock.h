#ifndef _ARCH_PVCLOCK_H_
#define _ARCH_PVCLOCK_H_

#include <lyos/type.h>

struct pvclock_vcpu_time_info {
    u32 version;
    u32 pad0;
    u64 tsc_timestamp;
    u64 system_time;
    u32 tsc_to_system_mul;
    s8 tsc_shift;
    u8 flags;
    u8 pad[2];
} __attribute__((__packed__));

struct pvclock_vsyscall_time_info {
    struct pvclock_vcpu_time_info pvti;
} __attribute__((__aligned__(64)));

#endif
