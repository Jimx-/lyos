.global syscall_gate_intr

.section .text

syscall_gate_intr:
    scall
    ret
