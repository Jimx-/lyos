
.global _start
.global ldso_bind_entry
.extern ldso_main

.section .text

_start:
	push	%ecx	# environ
	push 	%edx	# argv
	push	%eax	# argc

	push 	%ecx
	call 	__lyos_init
	addl 	$4, %esp
	
	call	ldso_main
	# don't need to clean up the stack here

	movl 	%eax, %ebx
	pop 	%eax
	pop 	%edx
	pop 	%ecx
	jmp 	*%ebx

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
