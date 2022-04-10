    .globl dcache_inval_poc

    .macro dcache_line_size, reg, tmp
    mrs     \tmp, ctr_el0
    ubfm    \tmp, \tmp, #16, #19
    mov     \reg, #4
    lsl     \reg, \reg, \tmp
    .endm

dcache_inval_poc:
    dcache_line_size x2, x3
    sub     x3, x2, #1
    tst     x1, x3
    bic     x1, x1, x3
    b.eq    1f
    dc  civac, x1
1:  tst     x0, x3
    bic     x0, x0, x3
    b.eq    2f
    dc  civac, x0
    b   3f
2:  dc  ivac, x0
3:  add     x0, x0, x2
    cmp     x0, x1
    b.lo    2b
    dsb     sy
    ret
