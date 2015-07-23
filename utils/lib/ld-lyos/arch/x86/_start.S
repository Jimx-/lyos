
.global _start
.global ldso_bind_entry
.extern ldso_main

.section .text

_start:
	push	%ecx	# environ
	push 	%edx	# argv
	push	%eax	# argc

	call 	__lyos_init
	
	call	ldso_main
	# don't need to clean up the stack here

	jmp 	*%eax

	push	%eax
	call	_exit

	hlt	# should never arrive here

ldso_bind_entry:
	pushf
	push 	%eax
	push	%ecx
	push 	%edx

	pushl 	20(%esp)
	pushl 	20(%esp)
	call 	ldso_bind
	addl	$8, %esp
	movl 	%eax, 20(%esp)

	pop 	%edx
	pop 	%ecx
	pop 	%eax
	popf

	leal	4(%esp), %esp
	ret
