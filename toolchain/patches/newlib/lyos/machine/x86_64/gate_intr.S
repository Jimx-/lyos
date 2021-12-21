.global syscall_gate_intr
.type syscall_gate_intr, @function

.section .text

syscall_gate_intr:
    push    %rbx
    mov     %rdi, %rax
    mov     %rsi, %rbx

    int     $0x90

    pop     %rbx
    ret
