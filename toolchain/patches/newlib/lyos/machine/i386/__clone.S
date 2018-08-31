.section .text

.global __clone

.equ STACK_PARENT,	1
.equ NR_SENDREC,	1

__clone:
    push %ebp
    movl %esp, %ebp

	push %ebx
	push $STACK_PARENT
	movl $NR_SENDREC, %eax	/* NR_SENDREC */
	movl 16(%esp), %ebx
	int $0x90

	lock; addl $0,0(%esp)

	mov (%esp), %ecx
	cmpl $STACK_PARENT, %ecx
	je __clone_parent

	movl 4(%esp), %eax	/* fn */
	push 8(%esp)	/* arg */
	call %eax

	call _exit

__clone_parent:
	add $4, %esp
	pop %ebx
    pop %ebp
	ret
