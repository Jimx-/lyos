#ifndef _SCONST_64_H_
#define _SCONST_64_H_

#define P_STACKBASE 0
#define R15REG      P_STACKBASE
#define R14REG      (R15REG + 8)
#define R13REG      (R14REG + 8)
#define R12REG      (R13REG + 8)
#define RBPREG      (R12REG + 8)
#define RBXREG      (RBPREG + 8)
#define R11REG      (RBXREG + 8)
#define R10REG      (R11REG + 8)
#define R9REG       (R10REG + 8)
#define R8REG       (R9REG + 8)
#define RAXREG      (R8REG + 8)
#define RCXREG      (RAXREG + 8)
#define RDXREG      (RCXREG + 8)
#define RSIREG      (RDXREG + 8)
#define RDIREG      (RSIREG + 8)
#define P_ORIGRAX   (RDIREG + 8)
#define RIPREG      (P_ORIGRAX + 8)
#define CSREG       (RIPREG + 8)
#define RFLAGSREG   (CSREG + 8)
#define RSPREG      (RFLAGSREG + 8)
#define SSREG       (RSPREG + 8)
#define KERNELSPREG (SSREG + 8)
#define RETADR      (KERNELSPREG + 8)
#define P_TRAPSTYLE (RETADR + 8)

/* clang-format off */

.macro CLEAR_IF where
    mov \where, %eax
    andl $0xfffffdff, %eax
    mov %eax, \where
.endm

.macro POP_REGS
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rax
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
.endm

.macro SAVE_GP_REGS base
    mov %r15, R15REG(\base)
    mov %r14, R14REG(\base)
    mov %r13, R13REG(\base)
    mov %r12, R12REG(\base)
    mov %rbx, RBXREG(\base)
    mov %r11, R11REG(\base)
    mov %r10, R10REG(\base)
    mov %r9, R9REG(\base)
    mov %r8, R8REG(\base)
    mov %rax, P_ORIGRAX(\base)
    mov %rax, RAXREG(\base)
    mov %rcx, RCXREG(\base)
    mov %rdx, RDXREG(\base)
    mov %rsi, RSIREG(\base)
    mov %rdi, RDIREG(\base)
.endm

.macro SAVE_CONTEXT offset, style
    // Recover proc pointer from the stack.
    push %rbp
    mov 48+\offset(%rsp), %rbp

    SAVE_GP_REGS %rbp

    pop %rsi
    mov %rsi, RBPREG(%rbp)
    mov %rsp, KERNELSPREG(%rbp)

    mov 0+\offset(%rsp), %rsi
    mov %rsi, RIPREG(%rbp)
    mov 8+\offset(%rsp), %rsi
    mov %rsi, CSREG(%rbp)
    mov 16+\offset(%rsp), %rsi
    mov %rsi, RFLAGSREG(%rbp)
    mov 24+\offset(%rsp), %rsi
    mov %rsi, RSPREG(%rbp)
    mov 32+\offset(%rsp), %rsi
    mov %rsi, SSREG(%rbp)

    movl $\style, P_TRAPSTYLE(%rbp)

    mov %rdx, %rsi
    mov %ss, %dx
    mov %dx, %ds

    mov %rsi, %rdx
    mov %rbp, %rsi
    xor %rbp, %rbp
.endm

/* clang-format on */

#endif
