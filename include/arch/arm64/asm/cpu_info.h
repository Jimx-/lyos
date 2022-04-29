#ifndef _ARCH_CPU_INFO_H_
#define _ARCH_CPU_INFO_H_

#include <asm/cpu_type.h>

struct cpu_info {
    unsigned long hwid;

    unsigned long reg_midr;
};

#endif
