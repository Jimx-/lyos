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

bits 32
[section .text]

global	out_byte
global	in_byte
global	out_word
global	in_word
global  out_long
global  in_long

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
