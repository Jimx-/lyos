.global syscall_gate_intr

.section .text

syscall_gate_intr:
	svc #0
	bx lr
