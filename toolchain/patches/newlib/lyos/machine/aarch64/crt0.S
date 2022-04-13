    .globl _start

    .section .text

    .type _start,%function
_start:
    mov x29, xzr
    mov x30, xzr

    mov x24, x0
    mov x25, x1
    mov x26, x2

    mov x0, sp
    and sp, x0, #-16

    mov x0, x2
    bl  __lyos_init

    mov x0, x24
    mov x1, x25
    mov x2, x26
    bl  main

    bl  _exit
