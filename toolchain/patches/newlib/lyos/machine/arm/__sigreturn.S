.global __sigreturn
.extern	sigreturn

.section .text

__sigreturn:
	pop 	{r0}
	b		sigreturn
