#include <asm/csr.h>

.section .text

.globl copy_user_message
.globl halt_cpu
.globl arch_pause

.globl __trunctfdf2

/* copy a message of 64 bytes */
copy_user_message:
    li t1, SR_SUM
    csrs sstatus, t1

    lw t0, 0(a1)
    sw t0, 0(a0)
    lw t0, 4(a1)
    sw t0, 4(a0)
    lw t0, 8(a1)
    sw t0, 8(a0)
    lw t0, 12(a1)
    sw t0, 12(a0)
    lw t0, 16(a1)
    sw t0, 16(a0)
    lw t0, 20(a1)
    sw t0, 20(a0)
    lw t0, 24(a1)
    sw t0, 24(a0)
    lw t0, 28(a1)
    sw t0, 28(a0)
    lw t0, 32(a1)
    sw t0, 32(a0)
    lw t0, 36(a1)
    sw t0, 36(a0)
    lw t0, 40(a1)
    sw t0, 40(a0)
    lw t0, 44(a1)
    sw t0, 44(a0)
    lw t0, 48(a1)
    sw t0, 48(a0)
    lw t0, 52(a1)
    sw t0, 52(a0)
    lw t0, 56(a1)
    sw t0, 56(a0)
    lw t0, 60(a1)
	sw t0, 60(a0)

    csrc sstatus, t1
    mv a0, x0
    ret

halt_cpu:
    fence
    csrs sstatus, SR_SIE
    wfi
    csrc sstatus, SR_SIE
    ret

arch_pause:
    nop
    ret

__trunctfdf2:
    /* some newlib routines require this */
    ret
