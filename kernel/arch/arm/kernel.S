.text

.global _start	/* entry point */

.extern cstart
.extern kernel_main

_start:
	ldr	sp, =StackTop
	mov fp, #0
	bl cstart

	ldr r2, =kernel_main
	bx r2

hang:
	b hang

.data
StackSpace:
	.space 4096
StackTop:
