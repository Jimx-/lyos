#ifndef _ARCH_FPU_H_
#define _ARCH_FPU_H_

struct fpu_state {
    u64 f[32];
    u32 fcsr;
};

#endif // _ARCH_FPU_H_
