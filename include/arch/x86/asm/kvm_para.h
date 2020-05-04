#ifndef _ARCH_KVM_PARA_H_
#define _ARCH_KVM_PARA_H_

#define KVM_CPUID_FEATURES 0x40000001
#define KVM_FEATURE_CLOCKSOURCE 0
#define KVM_FEATURE_NOP_IO_DELAY 1
#define KVM_FEATURE_MMU_OP 2
#define KVM_FEATURE_CLOCKSOURCE2 3

#define MSR_KVM_WALL_CLOCK 0x11
#define MSR_KVM_SYSTEM_TIME 0x12

#define KVM_MSR_ENABLED 1
/* Custom MSRs falls in the range 0x4b564d00-0x4b564dff */
#define MSR_KVM_WALL_CLOCK_NEW 0x4b564d00
#define MSR_KVM_SYSTEM_TIME_NEW 0x4b564d01

#ifdef CONFIG_KVM_GUEST
int kvm_para_available(void);
unsigned int kvm_arch_para_features(void);

void init_kvmclock();
#else
static inline int kvm_para_available(void) { return 0; }
unsigned int kvm_arch_para_features(void) { return 0; }

void init_kvmclock() {}
#endif

#endif
