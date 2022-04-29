#ifndef _ARCH_FPU_H_
#define _ARCH_FPU_H_

union fpelem {
    u64 u64[2];
};

struct fpu_state {
    union fpelem fp_reg[32];
    u32 fpcr;
    u32 fpsr;
    u64 __padding;
} __attribute__((aligned(16)));

void load_fpregs(const struct fpu_state* fpregs);
void save_fpregs(struct fpu_state* fpregs);

#endif // _ARCH_FPU_H_
