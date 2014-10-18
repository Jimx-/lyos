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
_NR_printx	    equ 0
_NR_sendrec	    equ 1
_NR_datacopy	equ 2
_NR_privctl		equ 3
_NR_getinfo 	equ 4
_NR_vmctl		equ 5

; 导出符号
global	printx
global	datacopy
global	_privctl
global	_getinfo
global  _vmctl

bits 32
[section .text]

; ====================================================================================
;                          void printx(MESSAGE * m);
; ====================================================================================
printx:
	push	edx		; 4 bytes

	mov	eax, _NR_printx
	mov	edx, [esp + 4 + 4]	; m
	int	INT_VECTOR_SYS_CALL

	pop	edx

	ret

;====================================================================================
;                          void data_copy(MESSAGE * m);
; ====================================================================================
datacopy:
	push	edx		; 4 bytes

	mov	eax, _NR_datacopy
	mov	edx, [esp + 4 + 4]	; m
	int	INT_VECTOR_SYS_CALL

	pop	edx

	ret

; ====================================================================================
;                  _privctl(int whom, int request, void* data);
; ====================================================================================
_privctl:
	push	edx		; /

	mov	eax, _NR_privctl
	mov edx, [esp + 4 + 4]
	int	INT_VECTOR_SYS_CALL

	pop	edx

	ret

; ====================================================================================
;                  getinfo(int request, void* buf);
; ====================================================================================
_getinfo:
	push	edx		; /

	mov	eax, _NR_getinfo
	mov	ecx, [esp + 4 +  4]
	int	INT_VECTOR_SYS_CALL

	pop	edx

	ret

; ====================================================================================
;                  vmctl(int request, int param);
; ====================================================================================
_vmctl:
	push	ecx		;  > 8 bytes
	push	edx		; /

	mov	eax, _NR_vmctl
	mov	ecx, [esp + 8 +  4]
	mov	edx, [esp + 8 +  8]
	int	INT_VECTOR_SYS_CALL

	pop	edx
	pop	ecx

	ret
