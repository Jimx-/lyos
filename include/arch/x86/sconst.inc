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

; 3.75G - 4G: kernel space
KERNEL_VMA	equ	0xF0000000
KERNEL_PDE	equ KERNEL_VMA >> 22

I386_CR0_WP     equ 0x00010000  ; Enable write-protection.
I386_CR0_PG     equ 0x80000000  ; Enable paging.
I386_CR4_PSE    equ 0x00000010  ; Page size extensions.
I386_CR4_PAE    equ 0x00000020  ; Physical addr extens.
I386_CR4_MCE    equ 0x00000040  ; Machine check enable.
I386_CR4_PGE    equ 0x00000080  ; Global page flag enable

P_LDT_SIZE	equ 2

P_STACKBASE	equ	0
GSREG		equ	P_STACKBASE
FSREG		equ	GSREG		+ 4
ESREG		equ	FSREG		+ 4
DSREG		equ	ESREG		+ 4
EDIREG		equ	DSREG		+ 4
ESIREG		equ	EDIREG		+ 4
EBPREG		equ	ESIREG		+ 4
KERNELESPREG	equ	EBPREG		+ 4
EBXREG		equ	KERNELESPREG	+ 4
EDXREG		equ	EBXREG		+ 4
ECXREG		equ	EDXREG		+ 4
EAXREG		equ	ECXREG		+ 4
RETADR		equ	EAXREG		+ 4
EIPREG		equ	RETADR		+ 4
CSREG		equ	EIPREG		+ 4
EFLAGSREG	equ	CSREG		+ 4
ESPREG		equ	EFLAGSREG	+ 4
SSREG		equ	ESPREG		+ 4
P_TRAPSTYLE	equ	SSREG		+ 4

INT_M_CTL	equ	0x20	; I/O port for interrupt controller         <Master>
INT_M_CTLMASK	equ	0x21	; setting bits in this port disables ints   <Master>
INT_S_CTL	equ	0xA0	; I/O port for second interrupt controller  <Slave>
INT_S_CTLMASK	equ	0xA1	; setting bits in this port disables ints   <Slave>

EOI		equ	0x20

;These selector must be the same as `protect.h'
SELECTOR_FLAT_C		equ		0x08		;
SELECTOR_KERNEL_CS	equ		SELECTOR_FLAT_C
SELECTOR_KERNEL_DS	equ		0x10
SELECTOR_USER_DS	equ		0x20

K_STACK_SIZE	equ	0x1000
CONFIG_SMP_MAX_CPUS	equ	8

; Kernel trap style
KTS_NONE		equ 0
KTS_INT			equ 1
KTS_SYSENTER 	equ 2

; Multiboot stuffs
MODULEALIGN equ  1<<0                   ; align loaded modules on page boundaries
MEMINFO   equ  1<<1                   ; provide memory map
FLAGS     equ  MODULEALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC       equ   0x1BADB002           ; 'magic number' lets bootloader find the header
CHECKSUM    equ -(MAGIC + FLAGS)        ; checksum required 

%macro	SAVE_GP_REGS		1
	mov [%1 + EAXREG], eax
	mov [%1 + ECXREG], ecx
	mov [%1 + EDXREG], edx
	mov [%1 + EBXREG], ebx
	mov [%1 + ESIREG], esi
	mov [%1 + EDIREG], edi
%endmacro

%macro	RESTORE_GP_REGS		1
	mov eax, [%1 + EAXREG]
	mov ecx, [%1 + ECXREG]
	mov edx, [%1 + EDXREG]
	mov ebx, [%1 + EBXREG]
	mov esi, [%1 + ESIREG]
	mov edi, [%1 + EDIREG]
%endmacro
