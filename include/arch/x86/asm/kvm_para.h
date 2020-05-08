#ifndef _ARCH_KVM_PARA_H_
#define _ARCH_KVM_PARA_H_

#include <lyos/type.h>

#define KVM_CPUID_FEATURES 0x40000001
#define KVM_FEATURE_CLOCKSOURCE 0
#define KVM_FEATURE_NOP_IO_DELAY 1
#define KVM_FEATURE_MMU_OP 2
#define KVM_FEATURE_CLOCKSOURCE2 3
#define KVM_FEATURE_STEAL_TIME 5

#define MSR_KVM_WALL_CLOCK 0x11
#define MSR_KVM_SYSTEM_TIME 0x12

#define KVM_MSR_ENABLED 1
/* Custom MSRs falls in the range 0x4b564d00-0x4b564dff */
#define MSR_KVM_WALL_CLOCK_NEW 0x4b564d00
#define MSR_KVM_SYSTEM_TIME_NEW 0x4b564d01
#define MSR_KVM_STEAL_TIME 0x4b564d03

struct kvm_steal_time {
    u64 steal;
    u32 version;
    u32 flags;
    u8 preempted;
    u8 u8_pad[3];
    u32 pad[11];
};

#ifdef CONFIG_KVM_GUEST
int kvm_para_available(void);
unsigned int kvm_arch_para_features(void);

void kvm_init_guest(void);
void kvm_init_guest_cpu(void);

u64 kvm_steal_clock(int cpu);

void init_kvmclock(void);
void kvm_register_clock(void);

#else
static inline int kvm_para_available(void) { return 0; }
static inline unsigned int kvm_arch_para_features(void) { return 0; }

static inline void kvm_init_guest(void) {}
static inline void kvm_init_guest_cpu(void) {}

static inline u64 kvm_steal_clock(int cpu) { return 0; }

static inline void init_kvmclock(void) {}
static inline void kvm_register_clock(void) {}

#endif

#endif
