.global syscall_gate_intr

.section .text

syscall_gate_intr:
	push %ebp
	movl %esp, %ebp

	push %ebx
	movl 12(%esp), %eax		/* syscall_nr */
	movl 16(%esp), %ebx		/* m */
	int	$0x90

	pop	%ebx
	pop %ebp

	ret
