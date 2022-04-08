#ifndef _UAPI_ASM_ARM64_SIGCONTEXT_H_
#define _UAPI_ASM_ARM64_SIGCONTEXT_H_

#include <lyos/types.h>

struct sigcontext {
    __u64 mask;
};

#endif
