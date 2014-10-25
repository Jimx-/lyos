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

; 导入全局变量
extern	disp_pos


[SECTION .text]

; 导出函数
global	out_byte
global	in_byte
global	out_word
global	in_word
global  out_long
global  in_long
global	enable_irq
global	disable_irq
global	enable_int
global	disable_int
global	port_read
global	port_write
global	glitter
global  arch_spinlock_lock
global  arch_spinlock_unlock
global  x86_lgdt
global  x86_lidt
global  x86_lldt
global 	x86_ltr
global	x86_load_ds
global	x86_load_es
global	x86_load_fs
global	x86_load_gs
global	x86_load_ss
global 	switch_k_stack
global  fninit
global  ia32_write_msr
global  halt_cpu
;global 	enable_paging
global  read_cr0
global  write_cr0
global 	read_cr2
global 	read_cr3
global  write_cr3
global	read_cr4
global 	write_cr4
global 	reload_cr3

; ========================================================================
;                  void port_read(u16 port, void* buf, int n);
; ========================================================================
port_read:
	mov	edx, [esp + 4]		; port
	mov	edi, [esp + 4 + 4]	; buf
	mov	ecx, [esp + 4 + 4 + 4]	; n
	shr	ecx, 1
	cld
	rep	insw
	ret

; ========================================================================
;                  void port_write(u16 port, void* buf, int n);
; ========================================================================
port_write:
	mov	edx, [esp + 4]		; port
	mov	esi, [esp + 4 + 4]	; buf
	mov	ecx, [esp + 4 + 4 + 4]	; n
	shr	ecx, 1
	cld
	rep	outsw
	ret

; ========================================================================
;		   void disable_irq(int irq);
; ========================================================================
; Disable an interrupt request line by setting an 8259 bit.
; Equivalent code:
;	if(irq < 8){
;		out_byte(INT_M_CTLMASK, in_byte(INT_M_CTLMASK) | (1 << irq));
;	}
;	else{
;		out_byte(INT_S_CTLMASK, in_byte(INT_S_CTLMASK) | (1 << irq));
;	}
disable_irq:
	mov	ecx, [esp + 4]		; irq
	pushf
	cli
	mov	ah, 1
	rol	ah, cl			; ah = (1 << (irq % 8))
	cmp	cl, 8
	jae	disable_8		; disable irq >= 8 at the slave 8259
disable_0:
	in	al, INT_M_CTLMASK
	test	al, ah
	jnz	dis_already		; already disabled?
	or	al, ah
	out	INT_M_CTLMASK, al	; set bit at master 8259
	popf
	mov	eax, 1			; disabled by this function
	ret
disable_8:
	in	al, INT_S_CTLMASK
	test	al, ah
	jnz	dis_already		; already disabled?
	or	al, ah
	out	INT_S_CTLMASK, al	; set bit at slave 8259
	popf
	mov	eax, 1			; disabled by this function
	ret
dis_already:
	popf
	xor	eax, eax		; already disabled
	ret

; ========================================================================
;		   void enable_irq(int irq);
; ========================================================================
; Enable an interrupt request line by clearing an 8259 bit.
; Equivalent code:
;	if(irq < 8){
;		out_byte(INT_M_CTLMASK, in_byte(INT_M_CTLMASK) & ~(1 << irq));
;	}
;	else{
;		out_byte(INT_S_CTLMASK, in_byte(INT_S_CTLMASK) & ~(1 << irq));
;	}
;
enable_irq:
	mov	ecx, [esp + 4]		; irq
	pushf
	cli
	mov	ah, ~1
	rol	ah, cl			; ah = ~(1 << (irq % 8))
	cmp	cl, 8
	jae	enable_8		; enable irq >= 8 at the slave 8259
enable_0:
	in	al, INT_M_CTLMASK
	and	al, ah
	out	INT_M_CTLMASK, al	; clear bit at master 8259
	popf
	ret
enable_8:
	in	al, INT_S_CTLMASK
	and	al, ah
	out	INT_S_CTLMASK, al	; clear bit at slave 8259
	popf
	ret

; ========================================================================
;		   void disable_int();
; ========================================================================
disable_int:
	cli
	ret

; ========================================================================
;		   void enable_int();
; ========================================================================
enable_int:
	sti
	ret

; ========================================================================
;                  void glitter(int row, int col);
; ========================================================================
glitter:
	push	eax
	push	ebx
	push	edx

	mov	eax, [.current_char]
	inc	eax
	cmp	eax, .strlen
	je	.1
	jmp	.2
.1:
	xor	eax, eax
.2:
	mov	[.current_char], eax
	mov	dl, byte [eax + .glitter_str]

	xor	eax, eax
	mov	al, [esp + 16]		; row
	mov	bl, .line_width
	mul	bl			; ax <- row * 80
	mov	bx, [esp + 20]		; col
	add	ax, bx
	shl	ax, 1
	movzx	eax, ax

	mov	[gs:eax], dl

	inc	eax
	mov	byte [gs:eax], 4

	jmp	.end

.current_char:	dd	0
.glitter_str:	db	'-\|/'
		db	'1234567890'
		db	'abcdefghijklmnopqrstuvwxyz'
		db	'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
.strlen		equ	$ - .glitter_str
.line_width	equ	80

.end:
	pop	edx
	pop	ebx
	pop	eax
	ret

; ========================================================================
;                  void arch_spinlock_lock(u32 * lock);
; ========================================================================
arch_spinlock_lock:
	mov	eax, [esp + 4]
	mov	edx, 1
.2:
	mov	ecx, 1
	xchg ecx, [eax]
	test ecx, ecx
	je .0

	cmp	edx, 1 << 16
	je	.1
	shl	edx, 1
.1:
	mov	ecx, edx
.3:
	pause
	sub ecx, 1
	test ecx, ecx
	jz .2
	jmp	.3
.0:
	mfence
	ret

; ========================================================================
;                  void arch_spinlock_unlock(u32 * lock);
; ========================================================================
arch_spinlock_unlock:
	mov	eax, [esp + 4]
	mov	ecx, 0
	xchg [eax], ecx
	mfence
	ret

x86_lgdt:
	push ebp
	mov ebp, esp
	mov eax, [ebp + 4 + 4]

	lgdt [eax]
	
	pop ebp
	ret

x86_lidt:
	push ebp
	mov ebp, esp
	mov eax, [ebp + 4 + 4]

	lidt [eax]
	
	pop ebp
	ret

x86_lldt:
	push ebp
	mov ebp, esp

	lldt [ebp + 4 + 4]
	
	pop ebp
	ret

x86_ltr:
	push ebp
	mov ebp, esp

	ltr [ebp + 4 + 4]
	
	pop ebp
	ret

%macro  load_from_ax	1
	push ebp
	mov ebp, esp
	xor eax, eax
	mov eax, [ebp + 4 + 4]

	mov %1, ax
	jmp .0
.0:
	pop ebp
	ret
%endmacro

%macro  load_from_eax	1
	push ebp
	mov ebp, esp
	xor eax, eax
	mov eax, [ebp + 4 + 4]

	mov %1, eax
	jmp .0
.0:
	pop ebp
	ret
%endmacro

x86_load_ds:
	load_from_ax	ds

x86_load_es:
	load_from_ax	es

x86_load_fs:
	load_from_ax	fs

x86_load_gs:
	load_from_ax	gs

x86_load_ss:
	load_from_ax	ss

read_ebp:
	mov eax, ebp
	ret
	
switch_k_stack:
	mov eax, [esp + 4 + 4]
	mov ecx, [esp + 4]
	xor ebp, ebp
	mov esp, ecx
	jmp eax
.0:
	jmp .0

fninit:
	fninit
	ret
	
ia32_write_msr:
	push ebp
	mov ebp, esp

	mov edx, [ebp + 12]
	mov eax, [ebp + 16]
	mov ecx, [ebp + 8]
	wrmsr

	pop ebp
	ret

halt_cpu:
	sti
	hlt
	ret

enable_paging:
	mov	eax, cr0
	or	eax, 80000000h
	mov	cr0, eax

	lea ecx, [paging_enabled]
	jmp ecx				; jump!
paging_enabled:
	nop

	ret

read_cr0:
    push ebp
    mov ebp, esp
    mov	eax, cr0
    pop ebp
	ret

write_cr0:
	load_from_eax	cr0

read_cr2:
    push ebp
    mov ebp, esp
    mov	eax, cr2
    pop ebp
	ret

read_cr3:
    push ebp
    mov ebp, esp
    mov	eax, cr3
    pop ebp
	ret

write_cr3:
	load_from_eax	cr3

read_cr4:
    push ebp
    mov ebp, esp
    mov	eax, cr4
    pop ebp
	ret

write_cr4:
	load_from_eax	cr4

reload_cr3:
	push ebp
	mov ebp, esp
	mov eax, cr3
	mov cr3, eax
	pop ebp
	ret
