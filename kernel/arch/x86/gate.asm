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

ALIGN 4

INT_VECTOR_SYS_CALL equ 0x90

[SECTION .usermapped]

global syscall_int
global syscall_sysenter

syscall_int:
	push ebp
	mov ebp, esp

	push ebx
	mov eax, [esp + 8 + 4]		; syscall_nr
	mov	ebx, [esp + 8 + 8]		; m
	int	INT_VECTOR_SYS_CALL

	pop	ebx
	pop ebp

	ret

syscall_sysenter:
	push ebp
	mov ebp, esp

	push ebx
	push ecx
	push edx

	mov eax, [esp + 16 + 4]		; syscall_nr
	mov	ebx, [esp + 16 + 8]		; m
	mov ecx, esp
	mov edx, .1

	sysenter
.1:
	pop edx
	pop ecx
	pop ebx
	pop ebp
	ret
