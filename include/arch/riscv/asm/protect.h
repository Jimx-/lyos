#ifndef _ARCH_PROTECT_H_
#define _ARCH_PROTECT_H_

#include <asm/stackframe.h>

struct tss {
    reg_t sp0;
};

#endif
