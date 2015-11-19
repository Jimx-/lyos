
.include "sconst.inc"

.section .text

.global restore_user_context
.global exc_vector_table
.extern svc_stack

.extern handle_sys_call
.extern switch_to_user
.extern stop_context
.extern idle_stop
.extern irq_entry_handle

.balign	4096
exc_vector_table:
	ldr pc, =invalid_exc			/* Reset */
	ldr pc, =undefined_inst_exc			/* Undefined Instruction */
	ldr pc, =svc_entry				/* Supervisor Call */
	ldr pc, =invalid_exc			/* Prefetch Abort */
	ldr pc, =invalid_exc			/* Data Abort */
	ldr pc, =invalid_exc			/* Hypervisor Call */
	ldr pc, =irq_entry				/* Interrupt */
	ldr pc, =invalid_exc			/* Fast Interrupt */

invalid_exc:
	b .

undefined_inst_exc:
	b .

irq_entry:
	switch_to_svc 4
	test_in_kernel irq_entry_kernel

	save_context

	ldr	fp, [sp]
	push	{fp}
	sub	sp, sp, #4

	mov	r0, fp
	mov	fp, #0
	bl	stop_context

	bl	irq_entry_handle

	add	sp, sp, #4
	pop	{fp}
	dsb

	b	switch_to_user

irq_entry_kernel:
	push	{r0-r12, lr}
	bl	idle_stop

	bl	irq_entry_handle

	dsb 
	pop	{r0-r12, lr}
	rfeia	sp!

svc_entry:
	srsdb	sp!, #PSR_SVC32_MODE
	save_context

	ldr	fp, [sp]

	push	{fp}
	push	{r1}
	push	{r0}

	mov	r0, fp
	mov	fp, #0
	bl	stop_context

	pop {r0}
	pop	{r1}
	pop	{r2}
	bl	handle_sys_call

	b	switch_to_user

restore_user_context:
	mov sp, r0

	ldr r0, [sp, #PSRREG]
	msr spsr_fsxc, r0
	ldr lr, [sp, #PCREG]

	ldm sp, {r0-r14}^

	ldr sp, =svc_stack
	ldr sp, [sp]

	movs pc, lr
