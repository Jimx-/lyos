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

extern  load_prot_selectors
extern  smp_boot_ap

extern StackTop
bits 32

global trampoline_32

[section .text]	; code is here
ALIGN 4

trampoline_32:
	mov ax, SELECTOR_KERNEL_DS
	mov ds, ax
	mov ss, ax
	mov esp, StackTop - 4

	call load_prot_selectors
	jmp smp_boot_ap
	hlt
