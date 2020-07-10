#ifndef _ARCH_PROTECT_H_
#define _ARCH_PROTECT_H_

#include "stackframe.h"

struct cpu_info {};

struct tss {
    reg_t sp0;
};

#endif
