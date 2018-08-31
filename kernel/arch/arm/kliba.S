.section .text

.global copy_user_message
.global halt_cpu
.global arch_pause

/* copy a message of 64 bytes */
copy_user_message:
	push	{r4-r10, lr}
	mov 	r9, r1
	mov 	r10, r0 
	/* first 32 bytes */
	ldm 	r9, {r0-r7}
	stm		r10, {r0-r7}
	/* next 32 bytes */
	add		r9, r9, #32
	add 	r10, r10, #32
	ldm		r9, {r0-r7}
	stm		r10, {r0-r7}

copy_user_message_end:
	pop 	{r4-r10, lr}
	mov		r0, #0
	bx 		lr

halt_cpu:
	dsb
	cpsie	i
	wfi
	cpsid 	i 
	bx 		lr

arch_pause:
	nop
	bx 		lr
	
