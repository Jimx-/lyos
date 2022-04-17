#include "ucontextoffsets.h"

    .section .text

    .extern resumecontext

    .global getcontext
    .global setcontext
    .global ctx_wrapper

getcontext:
    str xzr,      [x0, oX0 + 0 * REG_SIZE]
    stp x18, x19, [x0, oX0 + 18 * REG_SIZE]
    stp x20, x21, [x0, oX0 + 20 * REG_SIZE]
    stp x22, x23, [x0, oX0 + 22 * REG_SIZE]
    stp x24, x25, [x0, oX0 + 24 * REG_SIZE]
    stp x26, x27, [x0, oX0 + 26 * REG_SIZE]
    stp x28, x29, [x0, oX0 + 28 * REG_SIZE]
    str x30,      [x0, oX0 + 30 * REG_SIZE]

    str x30, [x0, oPC]

    mov x2, sp
    str x2, [x0, oSP]

    str xzr, [x0, oPSTATE]

    mov x0, 0
    ret

setcontext:
    ldp     x18, x19, [x0, oX0 + 18 * REG_SIZE]
    ldp     x20, x21, [x0, oX0 + 20 * REG_SIZE]
    ldp     x22, x23, [x0, oX0 + 22 * REG_SIZE]
    ldp     x24, x25, [x0, oX0 + 24 * REG_SIZE]
    ldp     x26, x27, [x0, oX0 + 26 * REG_SIZE]
    ldp     x28, x29, [x0, oX0 + 28 * REG_SIZE]
    ldr     x30,      [x0, oX0 + 30 * REG_SIZE]
    ldr     x2, [x0, oSP]
    mov     sp, x2

    ldr     x16, [x0, oPC]

    ldp     x2, x3, [x0, oX0 + 2 * REG_SIZE]
    ldp     x4, x5, [x0, oX0 + 4 * REG_SIZE]
    ldp     x6, x7, [x0, oX0 + 6 * REG_SIZE]
    ldp     x0, x1, [x0, oX0 + 0 * REG_SIZE]

    br      x16

ctx_wrapper:
    mov x0, x21
    b   resumecontext
