#include <asm/asm.h>
#include <asm/csr.h>
#include <sconst.h>

.section .text

.globl phys_copy
.globl phys_copy_fault
.globl phys_copy_fault_in_kernel
.globl copy_user_message
.globl copy_user_message_end
.globl copy_user_message_fault
.globl halt_cpu
.globl arch_pause

.globl __trunctfdf2

phys_copy:
    addi sp, sp, -(REG_SIZE)
    REG_S ra, 0(sp)

    mv t6, a0  /* Preserve return value */
    mv a0, zero

    /* Defer to byte-oriented copy for small sizes */
    sltiu a3, a2, 128
    bnez a3, 4f
    /* Use word-oriented copy only if low-order bits match */
    andi a3, t6, REG_SIZE-1
    andi a4, a1, REG_SIZE-1
    bne a3, a4, 4f

    beqz a3, 2f  /* Skip if already aligned */
    /*
     * Round to nearest double word-aligned address
     * greater than or equal to start address
     */
    andi a3, a1, ~(REG_SIZE-1)
    addi a3, a3, REG_SIZE
    /* Handle initial misalignment */
    sub a4, a3, a1
1:
    lb a5, 0(a1)
    addi a1, a1, 1
    sb a5, 0(t6)
    addi t6, t6, 1
    bltu a1, a3, 1b
    sub a2, a2, a4  /* Update count */

2:
    andi a4, a2, ~((16*REG_SIZE)-1)
    beqz a4, 4f
    add a3, a1, a4
3:
    REG_L a4,       0(a1)
    REG_L a5,   REG_SIZE(a1)
    REG_L a6, 2*REG_SIZE(a1)
    REG_L a7, 3*REG_SIZE(a1)
    REG_L t0, 4*REG_SIZE(a1)
    REG_L t1, 5*REG_SIZE(a1)
    REG_L t2, 6*REG_SIZE(a1)
    REG_L t3, 7*REG_SIZE(a1)
    REG_L t4, 8*REG_SIZE(a1)
    REG_L t5, 9*REG_SIZE(a1)
    REG_S a4,       0(t6)
    REG_S a5,   REG_SIZE(t6)
    REG_S a6, 2*REG_SIZE(t6)
    REG_S a7, 3*REG_SIZE(t6)
    REG_S t0, 4*REG_SIZE(t6)
    REG_S t1, 5*REG_SIZE(t6)
    REG_S t2, 6*REG_SIZE(t6)
    REG_S t3, 7*REG_SIZE(t6)
    REG_S t4, 8*REG_SIZE(t6)
    REG_S t5, 9*REG_SIZE(t6)
    REG_L a4, 10*REG_SIZE(a1)
    REG_L a5, 11*REG_SIZE(a1)
    REG_L a6, 12*REG_SIZE(a1)
    REG_L a7, 13*REG_SIZE(a1)
    REG_L t0, 14*REG_SIZE(a1)
    REG_L t1, 15*REG_SIZE(a1)
    addi a1, a1, 16*REG_SIZE
    REG_S a4, 10*REG_SIZE(t6)
    REG_S a5, 11*REG_SIZE(t6)
    REG_S a6, 12*REG_SIZE(t6)
    REG_S a7, 13*REG_SIZE(t6)
    REG_S t0, 14*REG_SIZE(t6)
    REG_S t1, 15*REG_SIZE(t6)
    addi t6, t6, 16*REG_SIZE
    bltu a1, a3, 3b
    andi a2, a2, (16*REG_SIZE)-1  /* Update count */

4:
    /* Handle trailing misalignment */
    beqz a2, phys_copy_fault
    add a3, a1, a2

    /* Use word-oriented copy if co-aligned to word boundary */
    or a5, a1, t6
    or a5, a5, a3
    andi a5, a5, 3
    bnez a5, 5f
7:
    lw a4, 0(a1)
    addi a1, a1, 4
    sw a4, 0(t6)
    addi t6, t6, 4
    bltu a1, a3, 7b

    j phys_copy_fault

5:
    lb a4, 0(a1)
    addi a1, a1, 1
    sb a4, 0(t6)
    addi t6, t6, 1
    bltu a1, a3, 5b
phys_copy_fault:
    REG_L ra, 0(sp)
    addi sp, sp, REG_SIZE
    ret
phys_copy_fault_in_kernel:
    REG_L ra, 0(sp)
    addi sp, sp, REG_SIZE
    csrr a0, sbadaddr
    ret

/* copy a message of 64 bytes */
copy_user_message:
    addi sp, sp, -(REG_SIZE)
    REG_S ra, 0(sp)

    li t1, SR_SUM
    csrs sstatus, t1

    ld t0, 0(a1)
    sd t0, 0(a0)
    ld t0, 8(a1)
    sd t0, 8(a0)
    ld t0, 16(a1)
    sd t0, 16(a0)
    ld t0, 24(a1)
    sd t0, 24(a0)
    ld t0, 32(a1)
    sd t0, 32(a0)
    ld t0, 40(a1)
    sd t0, 40(a0)
    ld t0, 48(a1)
    sd t0, 48(a0)
    ld t0, 56(a1)
    sd t0, 56(a0)
    ld t0, 64(a1)
    sd t0, 64(a0)
    ld t0, 72(a1)
    sd t0, 72(a0)
copy_user_message_end:
    csrc sstatus, t1
    REG_L ra, 0(sp)
    addi sp, sp, REG_SIZE
    mv a0, zero
    ret
copy_user_message_fault:
    li t1, SR_SUM
    csrc sstatus, t1
    REG_L ra, 0(sp)
    addi sp, sp, REG_SIZE
    li a0, 1
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
