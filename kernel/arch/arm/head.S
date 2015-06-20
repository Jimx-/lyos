.section .head

.global _lyos_start	/* entry point */
.global _paging_enabled
.extern cstart
.extern setup_paging
.extern kernel_main

_lyos_start:
	ldr r5, =0x49020000
	mov r6, #'1'
	strb r6, [r5]

	ldr	sp, =UnpagedStackTop	/* Initial stack */
	mov fp, #0

	push {r0-r2}
	bl setup_paging
	pop {r0-r2}

	ldr r5, =0x49020000
	mov r6, #'2'
	strb r6, [r5]

	ldr r3, =_paging_enabled
	bx r3

_failed:
	b _failed

.section .unpaged_data
UnpagedStackSpace:
	.space 4096
UnpagedStackTop:

.section .text
_paging_enabled:
ldr r5, =0x49020000;
    mov r6, #'3'
    strb r6, [r5]
	ldr sp, =StackTop
	bl 	cstart

	ldr r2, =kernel_main
	bx r2

hang:
	b hang

.section .data
StackSpace:
	.space 4096
StackTop:
