#ifndef _ARCH_CPU_INFO_H_
#define _ARCH_CPU_INFO_H_

struct cpu_info {
    unsigned int hart;
    char isa[20];
    char mmu[20];
};

#endif
