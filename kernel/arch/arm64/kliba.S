    .section .text

    .globl phys_copy
    .globl phys_copy_fault
    .globl phys_copy_fault_in_kernel
    .globl copy_user_message
    .globl copy_user_message_end
    .globl copy_user_message_fault
    .globl halt_cpu
    .globl arch_pause
    .globl load_fpregs
    .globl save_fpregs

    #define L(label) .L ## label

#define dstin   x0
#define src     x1
#define count   x2
#define dst     x3
#define srcend  x4
#define dstend  x5
#define A_l     x6
#define A_lw    w6
#define A_h     x7
#define B_l     x8
#define B_lw    w8
#define B_h     x9
#define C_l     x10
#define C_lw    w10
#define C_h     x11
#define D_l     x12
#define D_h     x13
#define E_l     x14
#define E_h     x15
#define F_l     x16
#define F_h     x17
#define G_l     count
#define G_h     dst
#define H_l     src
#define H_h     srcend
#define tmp1    x14

phys_copy:
    add     srcend, src, count
    add     dstend, dstin, count
    cmp     count, 128
    b.hi    L(copy_long)
    cmp     count, 32
    b.hi    L(copy32_128)

    /* Small copies: 0..32 bytes.  */
    cmp     count, 16
    b.lo    L(copy16)
    ldp     A_l, A_h, [src]
    ldp     D_l, D_h, [srcend, -16]
    stp     A_l, A_h, [dstin]
    stp     D_l, D_h, [dstend, -16]
    mov     x0, xzr
    ret

    /* Copy 8-15 bytes.  */
L(copy16):
    tbz     count, 3, L(copy8)
    ldr     A_l, [src]
    ldr     A_h, [srcend, -8]
    str     A_l, [dstin]
    str     A_h, [dstend, -8]
    mov     x0, xzr
    ret

    .p2align 3
    /* Copy 4-7 bytes.  */
L(copy8):
    tbz     count, 2, L(copy4)
    ldr     A_lw, [src]
    ldr     B_lw, [srcend, -4]
    str     A_lw, [dstin]
    str     B_lw, [dstend, -4]
    mov     x0, xzr
    ret

    /* Copy 0..3 bytes using a branchless sequence.  */
L(copy4):
    cbz     count, L(copy0)
    lsr     tmp1, count, 1
    ldrb    A_lw, [src]
    ldrb    C_lw, [srcend, -1]
    ldrb    B_lw, [src, tmp1]
    strb    A_lw, [dstin]
    strb    B_lw, [dstin, tmp1]
    strb    C_lw, [dstend, -1]
L(copy0):
    mov     x0, xzr
    ret

    .p2align 4
    /* Medium copies: 33..128 bytes.  */
L(copy32_128):
    ldp     A_l, A_h, [src]
    ldp     B_l, B_h, [src, 16]
    ldp     C_l, C_h, [srcend, -32]
    ldp     D_l, D_h, [srcend, -16]
    cmp     count, 64
    b.hi    L(copy128)
    stp     A_l, A_h, [dstin]
    stp     B_l, B_h, [dstin, 16]
    stp     C_l, C_h, [dstend, -32]
    stp     D_l, D_h, [dstend, -16]
    mov     x0, xzr
    ret

    .p2align 4
    /* Copy 65..128 bytes.  */
L(copy128):
    ldp     E_l, E_h, [src, 32]
    ldp     F_l, F_h, [src, 48]
    cmp     count, 96
    b.ls    L(copy96)
    ldp     G_l, G_h, [srcend, -64]
    ldp     H_l, H_h, [srcend, -48]
    stp     G_l, G_h, [dstend, -64]
    stp     H_l, H_h, [dstend, -48]
L(copy96):
    stp     A_l, A_h, [dstin]
    stp     B_l, B_h, [dstin, 16]
    stp     E_l, E_h, [dstin, 32]
    stp     F_l, F_h, [dstin, 48]
    stp     C_l, C_h, [dstend, -32]
    stp     D_l, D_h, [dstend, -16]
    mov     x0, xzr
    ret

    .p2align 4
    /* Copy more than 128 bytes.  */
L(copy_long):
    /* Use backwards copy if there is an overlap.  */
    sub     tmp1, dstin, src
    cbz     tmp1, L(copy0)
    cmp     tmp1, count
    b.lo    L(copy_long_backwards)

    /* Copy 16 bytes and then align dst to 16-byte alignment.  */

    ldp     D_l, D_h, [src]
    and     tmp1, dstin, 15
    bic     dst, dstin, 15
    sub     src, src, tmp1
    add     count, count, tmp1  /* Count is now 16 too large.  */
    ldp     A_l, A_h, [src, 16]
    stp     D_l, D_h, [dstin]
    ldp     B_l, B_h, [src, 32]
    ldp     C_l, C_h, [src, 48]
    ldp     D_l, D_h, [src, 64]!
    subs    count, count, 128 + 16  /* Test and readjust count.  */
    b.ls    L(copy64_from_end)

L(loop64):
    stp     A_l, A_h, [dst, 16]
    ldp     A_l, A_h, [src, 16]
    stp     B_l, B_h, [dst, 32]
    ldp     B_l, B_h, [src, 32]
    stp     C_l, C_h, [dst, 48]
    ldp     C_l, C_h, [src, 48]
    stp     D_l, D_h, [dst, 64]!
    ldp     D_l, D_h, [src, 64]!
    subs    count, count, 64
    b.hi    L(loop64)

    /* Write the last iteration and copy 64 bytes from the end.  */
L(copy64_from_end):
    ldp     E_l, E_h, [srcend, -64]
    stp     A_l, A_h, [dst, 16]
    ldp     A_l, A_h, [srcend, -48]
    stp     B_l, B_h, [dst, 32]
    ldp     B_l, B_h, [srcend, -32]
    stp     C_l, C_h, [dst, 48]
    ldp     C_l, C_h, [srcend, -16]
    stp     D_l, D_h, [dst, 64]
    stp     E_l, E_h, [dstend, -64]
    stp     A_l, A_h, [dstend, -48]
    stp     B_l, B_h, [dstend, -32]
    stp     C_l, C_h, [dstend, -16]
    mov     x0, xzr
    ret

    .p2align 4

    /* Large backwards copy for overlapping copies.
       Copy 16 bytes and then align dst to 16-byte alignment.  */
L(copy_long_backwards):
    ldp     D_l, D_h, [srcend, -16]
    and     tmp1, dstend, 15
    sub     srcend, srcend, tmp1
    sub     count, count, tmp1
    ldp     A_l, A_h, [srcend, -16]
    stp     D_l, D_h, [dstend, -16]
    ldp     B_l, B_h, [srcend, -32]
    ldp     C_l, C_h, [srcend, -48]
    ldp     D_l, D_h, [srcend, -64]!
    sub     dstend, dstend, tmp1
    subs    count, count, 128
    b.ls    L(copy64_from_start)

L(loop64_backwards):
    stp     A_l, A_h, [dstend, -16]
    ldp     A_l, A_h, [srcend, -16]
    stp     B_l, B_h, [dstend, -32]
    ldp     B_l, B_h, [srcend, -32]
    stp     C_l, C_h, [dstend, -48]
    ldp     C_l, C_h, [srcend, -48]
    stp     D_l, D_h, [dstend, -64]!
    ldp     D_l, D_h, [srcend, -64]!
    subs    count, count, 64
    b.hi    L(loop64_backwards)

    /* Write the last iteration and copy 64 bytes from the start.  */
L(copy64_from_start):
    ldp     G_l, G_h, [src, 48]
    stp     A_l, A_h, [dstend, -16]
    ldp     A_l, A_h, [src, 32]
    stp     B_l, B_h, [dstend, -32]
    ldp     B_l, B_h, [src, 16]
    stp     C_l, C_h, [dstend, -48]
    ldp     C_l, C_h, [src]
    stp     D_l, D_h, [dstend, -64]
    stp     G_l, G_h, [dstin, 48]
    stp     A_l, A_h, [dstin, 32]
    stp     B_l, B_h, [dstin, 16]
    stp     C_l, C_h, [dstin]
    mov     x0, xzr
phys_copy_fault:
    ret
phys_copy_fault_in_kernel:
    ret

copy_user_message:
    ldp x2, x3, [x1, #0]
    stp x2, x3, [x0, #0]
    ldp x2, x3, [x1, #16]
    stp x2, x3, [x0, #16]
    ldp x2, x3, [x1, #32]
    stp x2, x3, [x0, #32]
    ldp x2, x3, [x1, #48]
    stp x2, x3, [x0, #48]
    ldp x2, x3, [x1, #64]
    stp x2, x3, [x0, #64]
copy_user_message_end:
    mov x0, xzr
    ret
copy_user_message_fault:
    mov x0, #1
    ret

halt_cpu:
    dsb sy
    wfi
    msr daifclr, #3
    msr daifset, #3
    ret

arch_pause:
    ret

load_fpregs:
    ldp    q0, q1, [x0, #(0 * 16)]
    ldp    q2, q3, [x0, #(2 * 16)]
    ldp    q4, q5, [x0, #(4 * 16)]
    ldp    q6, q7, [x0, #(6 * 16)]
    ldp    q8, q9, [x0, #(8 * 16)]
    ldp    q10, q11, [x0, #(10 * 16)]
    ldp    q12, q13, [x0, #(12 * 16)]
    ldp    q14, q15, [x0, #(14 * 16)]
    ldp    q16, q17, [x0, #(16 * 16)]
    ldp    q18, q19, [x0, #(18 * 16)]
    ldp    q20, q21, [x0, #(20 * 16)]
    ldp    q22, q23, [x0, #(22 * 16)]
    ldp    q24, q25, [x0, #(24 * 16)]
    ldp    q26, q27, [x0, #(26 * 16)]
    ldp    q28, q29, [x0, #(28 * 16)]
    ldp    q30, q31, [x0, #(30 * 16)]
    ldr    w8, [x0, #(32 * 16 + 0)]
    ldr    w9, [x0, #(32 * 16 + 4)]
    msr    fpcr, x8
    msr    fpsr, x9
    ret

save_fpregs:
    stp    q0, q1, [x0, #(0 * 16)]
    stp    q2, q3, [x0, #(2 * 16)]
    stp    q4, q5, [x0, #(4 * 16)]
    stp    q6, q7, [x0, #(6 * 16)]
    stp    q8, q9, [x0, #(8 * 16)]
    stp    q10, q11, [x0, #(10 * 16)]
    stp    q12, q13, [x0, #(12 * 16)]
    stp    q14, q15, [x0, #(14 * 16)]
    stp    q16, q17, [x0, #(16 * 16)]
    stp    q18, q19, [x0, #(18 * 16)]
    stp    q20, q21, [x0, #(20 * 16)]
    stp    q22, q23, [x0, #(22 * 16)]
    stp    q24, q25, [x0, #(24 * 16)]
    stp    q26, q27, [x0, #(26 * 16)]
    stp    q28, q29, [x0, #(28 * 16)]
    stp    q30, q31, [x0, #(30 * 16)]
    mrs    x8, fpcr
    mrs    x9, fpsr
    str    w8, [x0, #(32 * 16 + 0)]
    str    w9, [x0, #(32 * 16 + 4)]
    ret
