#ifndef _ARCH_CPU_INFO_H_
#define _ARCH_CPU_INFO_H_

struct cpu_info {
    u8 vendor;
    u8 family;
    u8 model;
    u8 stepping;
    u32 freq_mhz;
    u32 flags[2];
};

#endif
