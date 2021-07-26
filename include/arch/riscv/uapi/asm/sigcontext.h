#ifndef _UAPI_ASM_RISCV_SIGCONTEXT_H_
#define _UAPI_ASM_RISCV_SIGCONTEXT_H_

#include <lyos/types.h>

struct sigcontext {
    __u64 mask;
};

#endif
