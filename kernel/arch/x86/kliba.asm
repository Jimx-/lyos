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
global	enable_int
global	disable_int
global	port_read
global	port_write
global  x86_lgdt
global  x86_lidt
global  x86_lldt
global 	x86_ltr
global  x86_load_kerncs
global	x86_load_ds
global	x86_load_es
global	x86_load_fs
global	x86_load_gs
global	x86_load_ss
global 	switch_k_stack
global  fninit
global  ia32_read_msr
global  ia32_write_msr
global  halt_cpu
global  read_cr0
global  write_cr0
global 	read_cr2
global 	read_cr3
global  write_cr3
global	read_cr4
global 	write_cr4
global 	reload_cr3
global  i8259_eoi_master
global  i8259_eoi_slave
global  arch_pause

; ========================================================================
;		   void out_byte(u16 port, u8 value);
; ========================================================================
out_byte:
	mov	edx, [esp + 4]		; port
	mov	al, [esp + 4 + 4]	; value
	out	dx, al
	nop	; 一点延迟
	nop
	ret

; ========================================================================
;		   u8 in_byte(u16 port);
; ========================================================================
in_byte:
	mov	edx, [esp + 4]		; port
	xor	eax, eax
	in	al, dx
	nop	; 一点延迟
	nop
	ret

; ========================================================================
;		   void out_word(u16 port, u16 value);
; ========================================================================
out_word:
	mov	edx, [esp + 4]		; port
	mov	ax, word [esp + 4 + 4]	; value
	out	dx, ax
	nop	; 一点延迟
	nop
	ret

; ========================================================================
;		   u16 in_word(u16 port);
; ========================================================================
in_word:
	mov	edx, [esp + 4]		; port
	xor	eax, eax
	in	ax, dx
	nop	; 一点延迟
	nop
	ret

; ========================================================================
;		   void out_long(u16 port, u32 value);
; ========================================================================
out_long:
	mov	edx, [esp + 4]		; port
	mov	eax, dword [esp + 4 + 4]	; value
	out	dx, eax
	nop	; 一点延迟
	nop
	ret

; ========================================================================
;		   u32 in_long(u16 port);
; ========================================================================
in_long:
	mov	edx, [esp + 4]		; port
	xor	eax, eax
	in	eax, dx
	nop	; 一点延迟
	nop
	ret

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

x86_load_kerncs:
	push ebp
	mov ebp, esp
	mov eax, [ebp + 8]
	jmp SELECTOR_KERNEL_CS:newcs
newcs:
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
	
ia32_read_msr:
	push ebp
	mov ebp, esp

	mov ecx, [ebp + 8]
	rdmsr
	mov ecx, [ebp + 12]
	mov [ecx], edx
	mov ecx, [ebp + 16]
	mov [ecx], eax

	pop ebp
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

i8259_eoi_master:
	mov	al, EOI				; `. Set EOI bit
	out	INT_M_CTL, al		; /
	ret

i8259_eoi_slave:
	mov	al, EOI			; `. 置EOI位(master)
	out	INT_M_CTL, al		; /
	nop				; `. 置EOI位(slave)
	out	INT_S_CTL, al		; /  一定注意：slave和master都要置EOI
	ret

arch_pause:
	pause
	ret
	