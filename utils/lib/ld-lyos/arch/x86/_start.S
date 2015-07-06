
.global _start
.extern ldso_main

.section .text

_start:
	push	%ecx	# environ
	push 	%edx	# argv
	push	%eax	# argc

	call 	__lyos_init
	
	call	ldso_main
	# don't need to clean up the stack here

	push	%eax
	call	_exit

	hlt	# should never arrive here
