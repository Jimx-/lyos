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

bits 32
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
global  x86_ltr
global  x86_load_kerncs
global	x86_load_ds
global	x86_load_es
global	x86_load_fs
global	x86_load_gs
global	x86_load_ss
global  switch_k_stack
global  fninit
global  fnstsw
global  fnstcw
global  fnsave
global  fxsave
global  xsave
global  frstor
global  fxrstor
global  xrstor
global  frstor_end
global  fxrstor_end
global  xrstor_end
global  frstor_fault
global  clts
global  ia32_read_msr
global  ia32_write_msr
global  halt_cpu
global  read_cr0
global  write_cr0
global  read_cr2
global  read_cr3
global  write_cr3
global	read_cr4
global  write_cr4
global  reload_cr3
global  i8259_eoi_master
global  i8259_eoi_slave
global  arch_pause
global  phys_copy
global  phys_copy_fault
global  phys_copy_fault_in_kernel
global  copy_user_message
global  copy_user_message_end
global  copy_user_message_fault

; ========================================================================
;		   void out_byte(u16 port, u8 value);
; ========================================================================
out_byte:
    mov	edx, [esp + 4]		; port
    mov	al, [esp + 4 + 4]	; value
    out	dx, al
    ret

; ========================================================================
;		   u8 in_byte(u16 port);
; ========================================================================
in_byte:
    mov	edx, [esp + 4]		; port
    xor	eax, eax
    in	al, dx
    ret

; ========================================================================
;		   void out_word(u16 port, u16 value);
; ========================================================================
out_word:
    mov	edx, [esp + 4]		; port
    mov	ax, word [esp + 4 + 4]	; value
    out	dx, ax
    ret

; ========================================================================
;		   u16 in_word(u16 port);
; ========================================================================
in_word:
    mov	edx, [esp + 4]		; port
    xor	eax, eax
    in	ax, dx
    ret

; ========================================================================
;		   void out_long(u16 port, u32 value);
; ========================================================================
out_long:
    mov	edx, [esp + 4]		; port
    mov	eax, dword [esp + 4 + 4]	; value
    out	dx, eax
    ret

; ========================================================================
;		   u32 in_long(u16 port);
; ========================================================================
in_long:
    mov	edx, [esp + 4]		; port
    xor	eax, eax
    in	eax, dx
    ret

; ========================================================================
;                  void port_read(u16 port, void* buf, int n);
; ========================================================================
port_read:
    push    ebp
    mov     ebp, esp
    push    edi

    mov     edx, [ebp + 8]		; port
    mov     edi, [ebp + 12]	; buf
    mov     ecx, [ebp + 16]	; n
    shr     ecx, 1

    cld
    rep	insw

    pop     edi
    pop     ebp
    ret

; ========================================================================
;                  void port_write(u16 port, void* buf, int n);
; ========================================================================
port_write:
    push    ebp
    mov     ebp, esp
    push    edi

    mov     edx, [ebp + 8]		; port
    mov     edi, [ebp + 12]	; buf
    mov     ecx, [ebp + 16]	; n
    shr     ecx, 1

    cld
    rep	outsw

    pop     edi
    pop     ebp
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

fnstsw:
    xor     eax, eax
    fnstsw  ax
    ret

fnstcw:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 8]
    fnstcw  [eax]

    pop     ebp
    ret

fnsave:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 8]
    fnsave  [eax]

    pop     ebp
    ret

fxsave:
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 8]
    fxsave  [eax]

    pop     ebp
    ret

xsave:
    push    ebp
    mov     ebp, esp

    mov     ecx, [ebp + 8]
    mov     edx, [ebp + 12]
    mov     eax, [ebp + 16]

    xsave   [ecx]

    pop     ebp
    ret

frstor:
    mov     eax, [esp + 4]
    frstor  [eax]
frstor_end:
    xor     eax, eax
    ret

fxrstor:
    mov     eax, [esp + 4]
    fxrstor [eax]
fxrstor_end:
    xor     eax, eax
    ret

xrstor:
    mov     ecx, [esp + 4]
    mov     edx, [esp + 8]
    mov     eax, [esp + 12]
    xrstor  [ecx]
xrstor_end:
    xor     eax, eax
    ret

frstor_fault:
    mov     eax, 1
    ret

clts:
    clts
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

phys_copy:
    push	ebp
    mov		ebp, esp

    cld
    push	esi
    push	edi

    mov		esi, [ebp + 12]
    mov     edi, [ebp + 8]
    mov     eax, [ebp + 16]

    cmp		eax, 10
    jb		pc_small
    ; align
    mov		ecx, esi
    neg		ecx
    and		ecx, 3
    sub		eax, ecx

    rep movsb
    mov		ecx, eax
    shr		ecx, 2	; count of dwords

    rep movsd
    and		eax, 3
pc_small:
    xchg	ecx, eax

    rep movsb

    mov		eax, 0
; kernel sends us here on fault
phys_copy_fault:
    pop		edi
    pop		esi
    pop		ebp
    ret
phys_copy_fault_in_kernel:
    pop		edi
    pop		esi
    pop		ebp
    mov		eax, cr2
    ret

; copy a user message of 64 bytes
copy_user_message:
    mov     ecx, [esp + 8]	; src
    mov     edx, [esp + 4]	; dest

    mov     eax, [ecx + 0*4]
    mov     [edx + 0*4], eax
    mov     eax, [ecx + 1*4]
    mov     [edx + 1*4], eax
    mov     eax, [ecx + 2*4]
    mov     [edx + 2*4], eax
    mov     eax, [ecx + 3*4]
    mov     [edx + 3*4], eax
    mov     eax, [ecx + 4*4]
    mov     [edx + 4*4], eax
    mov     eax, [ecx + 5*4]
    mov     [edx + 5*4], eax
    mov     eax, [ecx + 6*4]
    mov     [edx + 6*4], eax
    mov     eax, [ecx + 7*4]
    mov     [edx + 7*4], eax
    mov     eax, [ecx + 8*4]
    mov     [edx + 8*4], eax
    mov     eax, [ecx + 9*4]
    mov     [edx + 9*4], eax
    mov     eax, [ecx + 10*4]
    mov     [edx + 10*4], eax
    mov     eax, [ecx + 11*4]
    mov     [edx + 11*4], eax
    mov     eax, [ecx + 12*4]
    mov     [edx + 12*4], eax
    mov     eax, [ecx + 13*4]
    mov     [edx + 13*4], eax
    mov     eax, [ecx + 14*4]
    mov     [edx + 14*4], eax
    mov     eax, [ecx + 15*4]
    mov     [edx + 15*4], eax
    mov     eax, [ecx + 16*4]
    mov     [edx + 16*4], eax
    mov     eax, [ecx + 17*4]
    mov     [edx + 17*4], eax
    mov     eax, [ecx + 18*4]
    mov     [edx + 18*4], eax
    mov     eax, [ecx + 19*4]
    mov     [edx + 19*4], eax
copy_user_message_end:
    mov     eax, 0
    ret
; kernel send us here on fault
copy_user_message_fault:
    mov     eax, 1
    ret
