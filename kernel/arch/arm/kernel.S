.text

.global _start	/* entry point */

_start:
	ldr	sp, =StackTop
	bx cstart

hang:
	b hang

.data
StackSpace:
	.space 4096
StackTop:
