/*
 * Copyright (C) 2013 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <lyos/config.h>
#include <asm/asm.h>

    .global memset

memset:
    move t0, a0  /* Preserve return value */

    /* Defer to byte-oriented fill for small sizes */
    sltiu a3, a2, 16
    bnez a3, 4f

    /*
     * Round to nearest XLEN-aligned address
     * greater than or equal to start address
     */
    addi a3, t0, REG_SIZE-1
    andi a3, a3, ~(REG_SIZE-1)
    beq a3, t0, 2f  /* Skip if already aligned */
    /* Handle initial misalignment */
    sub a4, a3, t0
1:
    sb a1, 0(t0)
    addi t0, t0, 1
    bltu t0, a3, 1b
    sub a2, a2, a4  /* Update count */

2: /* Duff's device with 32 XLEN stores per iteration */
    /* Broadcast value into all bytes */
    andi a1, a1, 0xff
    slli a3, a1, 8
    or a1, a3, a1
    slli a3, a1, 16
    or a1, a3, a1
#ifdef CONFIG_64BIT
    slli a3, a1, 32
    or a1, a3, a1
#endif

    /* Calculate end address */
    andi a4, a2, ~(REG_SIZE-1)
    add a3, t0, a4

    andi a4, a4, 31*REG_SIZE  /* Calculate remainder */
    beqz a4, 3f            /* Shortcut if no remainder */
    neg a4, a4
    addi a4, a4, 32*REG_SIZE  /* Calculate initial offset */

    /* Adjust start address with offset */
    sub t0, t0, a4

    /* Jump into loop body */
    /* Assumes 32-bit instruction lengths */
    la a5, 3f
#ifdef CONFIG_64BIT
    srli a4, a4, 1
#endif
    add a5, a5, a4
    jr a5
3:
    REG_S a1,        0(t0)
    REG_S a1,    REG_SIZE(t0)
    REG_S a1,  2*REG_SIZE(t0)
    REG_S a1,  3*REG_SIZE(t0)
    REG_S a1,  4*REG_SIZE(t0)
    REG_S a1,  5*REG_SIZE(t0)
    REG_S a1,  6*REG_SIZE(t0)
    REG_S a1,  7*REG_SIZE(t0)
    REG_S a1,  8*REG_SIZE(t0)
    REG_S a1,  9*REG_SIZE(t0)
    REG_S a1, 10*REG_SIZE(t0)
    REG_S a1, 11*REG_SIZE(t0)
    REG_S a1, 12*REG_SIZE(t0)
    REG_S a1, 13*REG_SIZE(t0)
    REG_S a1, 14*REG_SIZE(t0)
    REG_S a1, 15*REG_SIZE(t0)
    REG_S a1, 16*REG_SIZE(t0)
    REG_S a1, 17*REG_SIZE(t0)
    REG_S a1, 18*REG_SIZE(t0)
    REG_S a1, 19*REG_SIZE(t0)
    REG_S a1, 20*REG_SIZE(t0)
    REG_S a1, 21*REG_SIZE(t0)
    REG_S a1, 22*REG_SIZE(t0)
    REG_S a1, 23*REG_SIZE(t0)
    REG_S a1, 24*REG_SIZE(t0)
    REG_S a1, 25*REG_SIZE(t0)
    REG_S a1, 26*REG_SIZE(t0)
    REG_S a1, 27*REG_SIZE(t0)
    REG_S a1, 28*REG_SIZE(t0)
    REG_S a1, 29*REG_SIZE(t0)
    REG_S a1, 30*REG_SIZE(t0)
    REG_S a1, 31*REG_SIZE(t0)
    addi t0, t0, 32*REG_SIZE
    bltu t0, a3, 3b
    andi a2, a2, REG_SIZE-1  /* Update count */

4:
    /* Handle trailing misalignment */
    beqz a2, 6f
    add a3, t0, a2
5:
    sb a1, 0(t0)
    addi t0, t0, 1
    bltu t0, a3, 5b
6:
    ret
