.section .text

.global _lyos_start	/* entry point */
.global restore_user_context

.extern cstart
.extern kernel_main

_lyos_start:
	ldr	sp, =StackTop
	mov fp, #0
	bl cstart

	ldr r2, =kernel_main
	bx r2

hang:
	b hang

restore_user_context:
	bx 	lr

.section .data
StackSpace:
	.space 4096
StackTop:
