.global syscall_gate_intr
.type syscall_gate_intr, @function

.section .text

syscall_gate_intr:
    push %rbp
    mov  %rsp, %rbp

    int	$0x90

    pop %rbp
    ret
