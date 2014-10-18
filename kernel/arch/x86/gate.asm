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

INT_VECTOR_SYS_CALL equ 0x90

[SECTION .usermapped]

global syscall_int

syscall_int:
	push ebx

	mov eax, [esp + 4 + 4]		; syscall_nr
	mov	ebx, [esp + 4 + 8]		; m
	int	INT_VECTOR_SYS_CALL

	pop	ebx

	ret
