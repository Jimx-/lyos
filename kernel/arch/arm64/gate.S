    .global syscall_svc

    .section .usermapped

syscall_svc:
    mov w8, w0
    mov x0, x1
    svc #0
    ret
