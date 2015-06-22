.section .text

.global __clone
.global __done

.equ STACK_PARENT,	1
.equ NR_SENDREC,	1

__clone:
	push %ebx
	push $STACK_PARENT
	movl $NR_SENDREC, %eax	/* NR_SENDREC */
	movl 12(%esp), %ebx
	int $0x90

	mov (%esp), %ecx
	cmpl $STACK_PARENT, %ecx
__done:
	je __clone_parent

	movl 4(%esp), %eax	/* fn */
	push 8(%esp)	/* arg */
	call %eax

	call _exit

__clone_parent:
	add $4, %esp
	pop %ebx
	ret
