.global __sigreturn
.extern	sigreturn

.section .text

__sigreturn:
	addl	$12, %esp
	jmp		sigreturn
