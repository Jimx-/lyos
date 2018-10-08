#ifndef _ARCH_ASM_H_
#define _ARCH_ASM_H_

#if __riscv_xlen == 64
#define __REG_SEL(a, b) a
#elif __riscv_xlen == 32
#define __REG_SEL(a, b) b
#else
#error "unexpected __riscv_xlen"
#endif

#define REG_L       __REG_SEL(ld, lw)
#define REG_SHIFT   __REG_SEL(3, 2)

#endif // _ARCH_ASM_H_
