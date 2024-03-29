#include <asm/asm.h>
#include <asm/csr.h>
#include <sconst.h>

.section .text

.global trap_entry
.global restore_user_context

.macro test_in_kernel   label
    csrr s0, sstatus
    andi s0, s0, SR_SPP
    bnez \label
.endm

.macro save_gregs base
    REG_S x1,  RAREG(\base)
    REG_S x3,  GPREG(\base)
    REG_S x5,  T0REG(\base)
    REG_S x6,  T1REG(\base)
    REG_S x7,  T2REG(\base)
    REG_S x8,  S0REG(\base)
    REG_S x9,  S1REG(\base)
    REG_S x10, A0REG(\base)
    REG_S x11, A1REG(\base)
    REG_S x12, A2REG(\base)
    REG_S x13, A3REG(\base)
    REG_S x14, A4REG(\base)
    REG_S x15, A5REG(\base)
    REG_S x16, A6REG(\base)
    REG_S x17, A7REG(\base)
    REG_S x18, S2REG(\base)
    REG_S x19, S3REG(\base)
    REG_S x20, S4REG(\base)
    REG_S x21, S5REG(\base)
    REG_S x22, S6REG(\base)
    REG_S x23, S7REG(\base)
    REG_S x24, S8REG(\base)
    REG_S x25, S9REG(\base)
    REG_S x26, S10REG(\base)
    REG_S x27, S11REG(\base)
    REG_S x28, T3REG(\base)
    REG_S x29, T4REG(\base)
    REG_S x30, T5REG(\base)
    REG_S x31, T6REG(\base)
.endm

.macro restore_gregs base
    REG_L x1,  RAREG(\base)
    REG_L x3,  GPREG(\base)
    REG_L x5,  T0REG(\base)
    REG_L x6,  T1REG(\base)
    REG_L x7,  T2REG(\base)
    REG_L x8,  S0REG(\base)
    REG_L x9,  S1REG(\base)
    REG_L x10, A0REG(\base)
    REG_L x11, A1REG(\base)
    REG_L x12, A2REG(\base)
    REG_L x13, A3REG(\base)
    REG_L x14, A4REG(\base)
    REG_L x15, A5REG(\base)
    REG_L x16, A6REG(\base)
    REG_L x17, A7REG(\base)
    REG_L x18, S2REG(\base)
    REG_L x19, S3REG(\base)
    REG_L x20, S4REG(\base)
    REG_L x21, S5REG(\base)
    REG_L x22, S6REG(\base)
    REG_L x23, S7REG(\base)
    REG_L x24, S8REG(\base)
    REG_L x25, S9REG(\base)
    REG_L x26, S10REG(\base)
    REG_L x27, S11REG(\base)
    REG_L x28, T3REG(\base)
    REG_L x29, T4REG(\base)
    REG_L x30, T5REG(\base)
    REG_L x31, T6REG(\base)
.endm

.macro save_context
    REG_S sp, SPREG(tp)
    REG_L sp, KERNELSPREG(tp)

    /* save all registers */
    save_gregs tp

    /* disable user memory access */
    li t0, SR_SUM
    csrrc s0, sstatus, t0

    /* cannot disable floating point yet because some newlib routines need it */
    li t0, SR_FS
    csrs sstatus, t0

    csrr s1, sepc
    csrr s2, sbadaddr
    csrr s3, scause
    csrr s4, sscratch

    REG_S s0, SSTATUSREG(tp)
    REG_S s1, SEPCREG(tp)
    REG_S s2, SBADADDRREG(tp)
    REG_S s3, SCAUSEREG(tp)
    REG_S s4, TPREG(tp)
.endm

.macro save_fstate
    li t0, SR_FS
    csrs sstatus, t0
    frcsr t1

    fsd f0, F0REG(tp)
    fsd f1, F1REG(tp)
    fsd f2, F2REG(tp)
    fsd f3, F3REG(tp)
    fsd f4, F4REG(tp)
    fsd f5, F5REG(tp)
    fsd f6, F6REG(tp)
    fsd f7, F7REG(tp)
    fsd f8, F8REG(tp)
    fsd f9, F9REG(tp)
    fsd f10, F10REG(tp)
    fsd f11, F11REG(tp)
    fsd f12, F12REG(tp)
    fsd f13, F13REG(tp)
    fsd f14, F14REG(tp)
    fsd f15, F15REG(tp)
    fsd f16, F16REG(tp)
    fsd f17, F17REG(tp)
    fsd f18, F18REG(tp)
    fsd f19, F19REG(tp)
    fsd f20, F20REG(tp)
    fsd f21, F21REG(tp)
    fsd f22, F22REG(tp)
    fsd f23, F23REG(tp)
    fsd f24, F24REG(tp)
    fsd f25, F25REG(tp)
    fsd f26, F26REG(tp)
    fsd f27, F27REG(tp)
    fsd f28, F28REG(tp)
    fsd f29, F29REG(tp)
    fsd f30, F30REG(tp)
    fsd f31, F31REG(tp)
    sw t1, FCSRREG(tp)

    csrc sstatus, t0
.endm

.macro restore_fstate
    li t0, SR_FS
    csrs sstatus, t0
    lw t1, FCSRREG(tp)

    fld f0, F0REG(tp)
    fld f1, F1REG(tp)
    fld f2, F2REG(tp)
    fld f3, F3REG(tp)
    fld f4, F4REG(tp)
    fld f5, F5REG(tp)
    fld f6, F6REG(tp)
    fld f7, F7REG(tp)
    fld f8, F8REG(tp)
    fld f9, F9REG(tp)
    fld f10, F10REG(tp)
    fld f11, F11REG(tp)
    fld f12, F12REG(tp)
    fld f13, F13REG(tp)
    fld f14, F14REG(tp)
    fld f15, F15REG(tp)
    fld f16, F16REG(tp)
    fld f17, F17REG(tp)
    fld f18, F18REG(tp)
    fld f19, F19REG(tp)
    fld f20, F20REG(tp)
    fld f21, F21REG(tp)
    fld f22, F22REG(tp)
    fld f23, F23REG(tp)
    fld f24, F24REG(tp)
    fld f25, F25REG(tp)
    fld f26, F26REG(tp)
    fld f27, F27REG(tp)
    fld f28, F28REG(tp)
    fld f29, F29REG(tp)
    fld f30, F30REG(tp)
    fld f31, F31REG(tp)
    fscsr t1

    csrc sstatus, t0

    li t0, SR_FS_CLEAN
    csrs sstatus, t0
.endm

    .align 4
trap_entry:
    /* sscratch == 0 if the call is from kernel, otherwise sscratch is a pointer to
     * the calling proc */
    csrrw tp, sscratch, tp
    bnez tp, trap_entry_user

    /* save all registers */
    addi sp, sp, -(SSTATUSREG)
    save_gregs sp

    /* restore tp from sscratch */
    csrr tp, sscratch
    csrr s0, sstatus
    csrr s1, sepc
    csrr s2, sbadaddr
    csrr s3, scause

    j fstate_saved

trap_entry_user:
    save_context

    li t1, SR_SD
    and t0, s0, t1
    beqz t0, fstate_saved
    save_fstate

fstate_saved:
    /* clear sscratch to indicate that we are already in kernel */
    csrw sscratch, x0

.option push
.option norelax
    /* load global pointer */
    la gp, __global_pointer$
.option pop

    mv s5, a0
    mv s6, a1
    mv a0, tp
    call stop_context
    mv a0, s5
    mv a1, s6

    /* check whether cause is interrupt or exception */
    bge s3, zero, do_exception
    j do_irq

do_irq:
    mv a0, s3
    call irq_entry_handle

    j ret_from_exception

do_exception:
    li t0, EXC_SYSCALL
    beq s3, t0, do_syscall

    andi a0, s0, SR_SPP     /* in_kernel */
    mv a1, tp               /* proc_ptr */

    slli t0, s3, RISCV_LGPTR
    la t1, exc_vector_table
    la t2, exc_vector_table_end
    add t0, t1, t0
    bgeu t1, t2, 1f

    REG_L t0, 0(t0)

    addi sp, sp, -(EF_SIZE)
    mv a2, sp
    REG_S s1, EF_SEPC(sp)
    REG_S s2, EF_SBADADDR(sp)
    REG_S s3, EF_SCAUSE(sp)

    jalr t0

    REG_L s1, EF_SEPC(sp)
    addi sp, sp, EF_SIZE

    j ret_from_exception

1:
    call do_trap_unknown
    tail switch_to_user

do_syscall:
    REG_S a0, P_ORIGA0(tp)

    /* skip the original scall instruction */
    addi s1, s1, 0x4
    REG_S s1, SEPCREG(tp)

    mv a2, tp
    call handle_sys_call

    tail switch_to_user

ret_from_exception:
    andi a0, s0, SR_SPP
    bnez a0, resume_kernel

    tail switch_to_user

resume_kernel:
    csrw sepc, s1

    restore_gregs sp
    addi sp, sp, SSTATUSREG

    sret

restore_user_context:
    mv tp, a0
    REG_L s0, SSTATUSREG(tp)
    andi s0, s0, SR_SPP
    bnez s0, restore_all

    /* save tp in sscratch only if we are returning to userspace */
    csrw sscratch, tp

restore_all:
    REG_L a0, SSTATUSREG(tp)
    REG_L a1, SEPCREG(tp)
    csrw sstatus, a0
    csrw sepc, a1

    restore_fstate

    restore_gregs tp

    REG_L x2,  SPREG(tp)
    REG_L x4,  TPREG(tp)

    sret

exc_vector_table:
    RISCV_PTR do_trap_insn_misaligned
    RISCV_PTR do_trap_insn_fault
    RISCV_PTR do_trap_insn_illegal
    RISCV_PTR do_trap_break
    RISCV_PTR do_trap_load_misaligned
    RISCV_PTR do_trap_load_fault
    RISCV_PTR do_trap_store_misaligned
    RISCV_PTR do_trap_store_fault
    RISCV_PTR do_trap_ecall_u
    RISCV_PTR do_trap_ecall_s
    RISCV_PTR do_trap_unknown
    RISCV_PTR do_trap_ecall_m
    RISCV_PTR do_page_fault
    RISCV_PTR do_page_fault
    RISCV_PTR do_trap_unknown
    RISCV_PTR do_page_fault
exc_vector_table_end:
