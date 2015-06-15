.section .init, "ax", %progbits
.global _init
.type _init,%function
_init:
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

.section .fini, "ax", %progbits
.global _fini
.type _fini,%function
_fini:
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4