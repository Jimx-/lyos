#include "ucontextoffsets.h"

    .section .text

    .extern resumecontext

    .global getcontext
    .global setcontext
    .global ctx_wrapper

getcontext:
    REG_S ra, PC(a0)
    REG_S ra, X1(a0)
    REG_S sp, X2(a0)
    REG_S s0, X8(a0)
    REG_S s1, X9(a0)
    REG_S x0, X10(a0)
    REG_S s2, X18(a0)
    REG_S s3, X19(a0)
    REG_S s4, X20(a0)
    REG_S s5, X21(a0)
    REG_S s6, X22(a0)
    REG_S s7, X23(a0)
    REG_S s8, X24(a0)
    REG_S s9, X25(a0)
    REG_S s10, X26(a0)
    REG_S s11, X27(a0)

    mv a0, zero /* Return zero. */
    ret

setcontext:
    mv t0, a0

    REG_L t1, PC(t0)
    REG_L ra, X1(t0)
    REG_L sp, X2(t0)
    REG_L s0, X8(t0)
    REG_L s1, X9(t0)
    REG_L a1, X11(t0)
    REG_L a2, X12(t0)
    REG_L a3, X13(t0)
    REG_L a4, X14(t0)
    REG_L a5, X15(t0)
    REG_L a6, X16(t0)
    REG_L a7, X17(t0)
    REG_L s2, X18(t0)
    REG_L s3, X19(t0)
    REG_L s4, X20(t0)
    REG_L s5, X21(t0)
    REG_L s6, X22(t0)
    REG_L s7, X23(t0)
    REG_L s8, X24(t0)
    REG_L s9, X25(t0)
    REG_L s10, X26(t0)
    REG_L s11, X27(t0)

    jr t1

ctx_wrapper:
    jalr s2

    mv a0, s1
    call resumecontext
    ret
