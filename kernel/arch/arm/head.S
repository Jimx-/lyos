.include "sconst.inc"

.section .head

.global _lyos_start	/* entry point */
.global _paging_enabled
.global k_stacks_start
.extern cstart
.extern setup_paging
.extern kernel_main

_lyos_start:
	ldr	sp, =UnpagedStackTop	/* initial stack */
	mov fp, #0

	push {r0-r2}
	bl setup_paging
	pop {r0-r2}

	/* jump to higher kernel */
	ldr r3, =_paging_enabled
	mov pc, r3

_failed:
	b _failed

.section .unpaged_data
UnpagedStackSpace:
	.space K_STACK_SIZE
UnpagedStackTop:

.section .text
_paging_enabled:
	ldr sp, =StackTop
	bl 	cstart

	ldr r2, =kernel_main
	bx r2

hang:
	b hang

.section .data
StackSpace:
	.space K_STACK_SIZE
StackTop:
.balign	K_STACK_SIZE
k_stacks_start:
	.space 2 * K_STACK_SIZE * 8
k_stacks_end:
