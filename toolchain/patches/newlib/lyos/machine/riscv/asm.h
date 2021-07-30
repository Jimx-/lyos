#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

#if __riscv_xlen == 64
#define __REG_SEL(a, b) a
#elif __riscv_xlen == 32
#define __REG_SEL(a, b) b
#else
#error "unexpected __riscv_xlen"
#endif

#define REG_L     __REG_SEL(ld, lw)
#define REG_S     __REG_SEL(sd, sw)
#define REG_SIZE  __REG_SEL(8, 4)
#define REG_SHIFT __REG_SEL(3, 2)

#if __SIZEOF_POINTER__ == 8
#define RISCV_PTR   .dword
#define RISCV_LGPTR 3
#elif __SIZEOF_POINTER == 4
#define RISCV_PTR   .word
#define RISCV_LGPTR 2
#endif

#endif // _MACHINE_ASM_H_
