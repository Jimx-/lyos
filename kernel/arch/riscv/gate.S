    .global syscall_scall

    .section .usermapped

syscall_scall:
    scall
    ret
