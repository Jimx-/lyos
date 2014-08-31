;    This file is part of Lyos.
; 
;    Lyos is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.
;
;    Lyos is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

global enable_paging
global read_cr2
global read_cr3
global reload_cr3

enable_paging:
	mov	eax, cr0
	or	eax, 80000000h
	mov	cr0, eax

	lea ecx, [paging_enabled]
	jmp ecx				; jump!
paging_enabled:
	nop

	ret

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

reload_cr3:
	push ebp
	mov ebp, esp
	mov eax, cr3
	mov cr3, eax
	pop ebp
	ret
