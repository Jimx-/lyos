;    This file is part of Lyos.
;
;    Lyos is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.

;    Lyos is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

%include "sconst.inc"

extern	cstart
extern	kernel_main
extern  switch_to_user
extern	exception_handler
extern	spurious_irq
extern  idle_stop
extern	clock_handler
extern	disp_str
extern	delay
extern	irq_table

extern	gdt_ptr
extern	idt_ptr
extern	current
extern	tss
extern	sys_call_table
bits 32

global pgd0
global StackTop
global k_stacks_start
global k_stacks_end

[SECTION .data]
ALIGN 0x1000
pgd0:
; 0x00000000
; pt0 here is the offset instead of the virtual address of pt0,
; so this can cause bugs
; TODO: initialize this in runtime or give pt0 a fixed address 
dd 				pt0 - KERNEL_VMA + 0x007
dd 				pt0 - KERNEL_VMA + 0x1007
dd 				pt0 - KERNEL_VMA + 0x2007
dd 				pt0 - KERNEL_VMA + 0x3007
times 			KERNEL_PDE - 4		dd 	0
; 0xF0000000
dd 				pt0 - KERNEL_VMA + 0x007
dd 				pt0 - KERNEL_VMA + 0x1007
dd 				pt0 - KERNEL_VMA + 0x2007
dd 				pt0 - KERNEL_VMA + 0x3007
times			1024 - KERNEL_PDE - 4	 	dd  0
clock_int_msg		db	"^", 0

[SECTION .bss]
ALIGN 0x1000
pt0:
page_tables		resb 	4 * 4 * 1024
StackSpace		resb	K_STACK_SIZE
StackTop:		; stack top
k_stacks_start:
KStacksSpace		resb 	2 * K_STACK_SIZE * CONFIG_SMP_MAX_CPUS
k_stacks_end:

[section .text]	; code is here
ALIGN 4

; Multiboot Header - We are multiboot compatible!
MultiBootHeader:
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

global _start	; export _start

global restore_user_context_int
global sys_call
global sys_call_sysenter

global	divide_error
global	single_step_exception
global	nmi
global	breakpoint_exception
global	overflow
global	bounds_check
global	inval_opcode
global	copr_not_available
global	double_fault
global	copr_seg_overrun
global	inval_tss
global	segment_not_present
global	stack_exception
global	general_protection
global 	page_fault
global	copr_error
global	hwint00
global	hwint01
global	hwint02
global	hwint03
global	hwint04
global	hwint05
global	hwint06
global	hwint07
global	hwint08
global	hwint09
global	hwint10
global	hwint11
global	hwint12
global	hwint13
global	hwint14
global	hwint15

_start:
	; setup initial page directory
	mov ecx, 4096	; 4096 page table entries
	mov edi, pt0 - KERNEL_VMA	; data segment: pt0 
	xor eax, eax
	mov eax, 0x007	; PRESENT | RW | USER
.1:
	stosd
	add	eax, 4096		; 4k per page
	loop	.1

	mov eax, pgd0 - KERNEL_VMA
	mov cr3, eax

	mov	eax, cr0
	or	eax, 80000000h
	mov	cr0, eax

	lea ecx, [paging_enabled]
	jmp ecx				; jump!
paging_enabled:
	; set stack
	mov	esp, StackTop	; stack is in section bss

	; Multiboot information
	push eax          
   	push ebx    

	call	cstart

	jmp	kernel_main

	hlt 	; should never arrive here


; interrupt and exception - hardware interrupt
; ---------------------------------
%macro	hwint_master	1
	cmp dword [esp + 4], SELECTOR_KERNEL_CS		; Test if this interrupt is triggered in kernel
	je .1
	call	save
	in	al, INT_M_CTLMASK	; `.
	or	al, (1 << %1)		;  | Mask current interrupt
	out	INT_M_CTLMASK, al	; /
	mov	al, EOI				; `. Set EOI bit
	out	INT_M_CTL, al		; /
	sti	; enable interrupt
	push	%1						; `.
	call	[irq_table + 4 * %1]	;  | Call the interrupt handler
	pop	ecx							; /
	cli
	in	al, INT_M_CTLMASK	; `.
	and	al, ~(1 << %1)		;  | Resume
	out	INT_M_CTLMASK, al	; /
	jmp switch_to_user
.1:
	pushad
	call idle_stop
	in	al, INT_S_CTLMASK	; `.
	or	al, (1 << (%1 - 8))	;  | 屏蔽当前中断
	out	INT_S_CTLMASK, al	; /
	mov	al, EOI			; `. 置EOI位(master)
	out	INT_M_CTL, al		; /
	nop				; `. 置EOI位(slave)
	out	INT_S_CTL, al		; /  一定注意：slave和master都要置EOI
	sti	; CPU在响应中断的过程中会自动关中断，这句之后就允许响应新的中断
	push	%1			; `.
	call	[irq_table + 4 * %1]	;  | 中断处理程序
	pop	ecx			; /
	cli
	in	al, INT_S_CTLMASK	; `.
	and	al, ~(1 << (%1 - 8))	;  | 恢复接受当前中断
	out	INT_S_CTLMASK, al	; /
	popad
	iret
%endmacro


ALIGN	16
hwint00:		; Interrupt routine for irq 0 (the clock).
	hwint_master	0

ALIGN	16
hwint01:		; Interrupt routine for irq 1 (keyboard)
	hwint_master	1

ALIGN	16
hwint02:		; Interrupt routine for irq 2 (cascade!)
	hwint_master	2

ALIGN	16
hwint03:		; Interrupt routine for irq 3 (second serial)
	hwint_master	3

ALIGN	16
hwint04:		; Interrupt routine for irq 4 (first serial)
	hwint_master	4

ALIGN	16
hwint05:		; Interrupt routine for irq 5 (XT winchester)
	hwint_master	5

ALIGN	16
hwint06:		; Interrupt routine for irq 6 (floppy)
	hwint_master	6

ALIGN	16
hwint07:		; Interrupt routine for irq 7 (printer)
	hwint_master	7

; ---------------------------------
%macro	hwint_slave	1
	cmp dword [esp + 4], SELECTOR_KERNEL_CS
	je .1
	call	save
	in	al, INT_S_CTLMASK	; `.
	or	al, (1 << (%1 - 8))	;  | 屏蔽当前中断
	out	INT_S_CTLMASK, al	; /
	mov	al, EOI			; `. 置EOI位(master)
	out	INT_M_CTL, al		; /
	nop				; `. 置EOI位(slave)
	out	INT_S_CTL, al		; /  一定注意：slave和master都要置EOI
	sti	; CPU在响应中断的过程中会自动关中断，这句之后就允许响应新的中断
	push	%1			; `.
	call	[irq_table + 4 * %1]	;  | 中断处理程序
	pop	ecx			; /
	cli
	in	al, INT_S_CTLMASK	; `.
	and	al, ~(1 << (%1 - 8))	;  | 恢复接受当前中断
	out	INT_S_CTLMASK, al	; /
	jmp switch_to_user
.1:
	pushad
	call idle_stop
	in	al, INT_S_CTLMASK	; `.
	or	al, (1 << (%1 - 8))	;  | 屏蔽当前中断
	out	INT_S_CTLMASK, al	; /
	mov	al, EOI			; `. 置EOI位(master)
	out	INT_M_CTL, al		; /
	nop				; `. 置EOI位(slave)
	out	INT_S_CTL, al		; /  一定注意：slave和master都要置EOI
	sti	; CPU在响应中断的过程中会自动关中断，这句之后就允许响应新的中断
	push	%1			; `.
	call	[irq_table + 4 * %1]	;  | 中断处理程序
	pop	ecx			; /
	cli
	in	al, INT_S_CTLMASK	; `.
	and	al, ~(1 << (%1 - 8))	;  | 恢复接受当前中断
	out	INT_S_CTLMASK, al	; /
	popad
	iret
%endmacro
; ---------------------------------

ALIGN	16
hwint08:		; Interrupt routine for irq 8 (realtime clock).
	hwint_slave	8

ALIGN	16
hwint09:		; Interrupt routine for irq 9 (irq 2 redirected)
	hwint_slave	9

ALIGN	16
hwint10:		; Interrupt routine for irq 10
	hwint_slave	10

ALIGN	16
hwint11:		; Interrupt routine for irq 11
	hwint_slave	11

ALIGN	16
hwint12:		; Interrupt routine for irq 12
	hwint_slave	12

ALIGN	16
hwint13:		; Interrupt routine for irq 13 (FPU exception)
	hwint_slave	13

ALIGN	16
hwint14:		; Interrupt routine for irq 14 (AT winchester)
	hwint_slave	14

ALIGN	16
hwint15:		; Interrupt routine for irq 15
	hwint_slave	15



; 中断和异常 -- 异常
divide_error:
	push	0xFFFFFFFF	; no err code
	push	0		; vector_no	= 0
	jmp	exception
single_step_exception:
	push	0xFFFFFFFF	; no err code
	push	1		; vector_no	= 1
	jmp	exception
nmi:
	push	0xFFFFFFFF	; no err code
	push	2		; vector_no	= 2
	jmp	exception
breakpoint_exception:
	push	0xFFFFFFFF	; no err code
	push	3		; vector_no	= 3
	jmp	exception
overflow:
	push	0xFFFFFFFF	; no err code
	push	4		; vector_no	= 4
	jmp	exception
bounds_check:
	push	0xFFFFFFFF	; no err code
	push	5		; vector_no	= 5
	jmp	exception
inval_opcode:
	push	0xFFFFFFFF	; no err code
	push	6		; vector_no	= 6
	jmp	exception
copr_not_available:
	push	0xFFFFFFFF	; no err code
	push	7		; vector_no	= 7
	jmp	exception
double_fault:
	push	8		; vector_no	= 8
	jmp	exception
copr_seg_overrun:
	push	0xFFFFFFFF	; no err code
	push	9		; vector_no	= 9
	jmp	exception
inval_tss:
	push	10		; vector_no	= A
	jmp	exception
segment_not_present:
	push	11		; vector_no	= B
	jmp	exception
stack_exception:
	push	12		; vector_no	= C
	jmp	exception
general_protection:
	push	13		; vector_no	= D
	jmp	exception
page_fault:
	push 	14 		; vector no = E
	jmp exception
copr_error:
	push	0xFFFFFFFF	; no err code
	push	16		; vector_no	= 10h
	jmp	exception

exception:
	cmp dword [esp + 4], SELECTOR_KERNEL_CS
	je exception_in_kernel

	call save_exception

	push esp 	; exception stack frame
	push 0 		; not in kernel
	call	exception_handler
	
	jmp switch_to_user
	
exception_in_kernel:
	pushad
	mov eax, esp
	add eax, 8 * 4 	; 8 registers

	push eax 	; exception stack frame
	push 1 		; in kernel
	call exception_handler
	add esp, 8
	popad

	add esp, 8 	; err_code and vec_no

	iretd

%macro	SAVE_CONTEXT	2
	push ebp
	mov ebp, [esp + 20 + 4 + %1]
	SAVE_GP_REGS	ebp
	pop esi

	mov [ebp + EBPREG], esi
	mov [ebp + KERNELESPREG], esp
	mov [ebp + DSREG], ds
	mov [ebp + ESREG], es
	mov [ebp + FSREG], fs
	mov [ebp + GSREG], gs
    
    mov esi, [esp + %1]
    mov [ebp + EIPREG], esi 
    mov esi, [esp + %1 + 4]
    mov [ebp + CSREG], esi 
    mov esi, [esp + %1 + 8]
    mov [ebp + EFLAGSREG], esi 
    mov esi, [esp + %1 + 12]
    mov [ebp + ESPREG], esi 
    mov esi, [esp + %1 + 16]
    mov [ebp + SSREG], esi

    mov dword [ebp + P_TRAPSTYLE], %2

	mov	esi, edx
	mov	dx, ss
	mov	ds, dx
	mov	es, dx
	mov	fs, dx

	mov	edx, esi	; 恢复 edx

    mov     esi, ebp                    ;esi = 进程表起始地址

    ret
%endmacro

; =============================================================================
;                                   save
; =============================================================================
save:
	SAVE_CONTEXT	4, KTS_INT

; =============================================================================
;                              save_exception
; =============================================================================
save_exception:
	SAVE_CONTEXT	12, KTS_INT

; =============================================================================
;                                 sys_call
; =============================================================================
sys_call:
    call    save

    sti

	push	esi
	push	ebx
    call    [sys_call_table + eax * 4]
	add	esp, 4 		; esp <- esi(proc ptr)

	pop esi
    mov     [esi + EAXREG - P_STACKBASE], eax
    cli

    jmp switch_to_user

; =============================================================================
;                            sys_call_sysenter
; =============================================================================
sys_call_sysenter:
	ret

; ====================================================================================
;                                   restore_user_context
; ====================================================================================
restore_user_context_int:
	mov	ebp, [esp + 4]
	
	; restore all registers
	mov ds, [ebp + DSREG]
	mov es, [ebp + ESREG]
	mov fs, [ebp + FSREG]
	mov gs, [ebp + GSREG]
	RESTORE_GP_REGS  ebp
	mov esp, ebp
	mov ebp, [ebp + EBPREG]

	; esp <- stack for iretd
	add esp, EIPREG
	iretd
