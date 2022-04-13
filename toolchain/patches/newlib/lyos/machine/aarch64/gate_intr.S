    .globl syscall_gate_intr

    .section .text

    .type syscall_gate_intr,%function
syscall_gate_intr:
    mov w8, w0
    mov x0, x1
    svc #0
    ret
