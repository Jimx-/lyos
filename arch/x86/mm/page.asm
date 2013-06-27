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
    
extern wp_page
extern no_page

;This routine handle int 14(page fault)
global page_fault

page_fault:
	xchg esp, dword	[eax]
	push dword	[ecx]
	push dword	[edx]
	push ds
	push es
	push fs
	mov edx, dword [0x10]
	mov ds, dx
	mov es, dx
	mov fs, dx
	mov edx, dword cr2
	push dword [edx]
	push dword [eax]
	test eax, dword	[1]
	jne	.1
	call no_page
	jmp	.2
.1:	call wp_page
.2:	add esp, dword	[8]
	pop fs
	pop es
	pop ds
	pop dword	[edx]
	pop dword	[ecx]
	pop dword	[eax]
	iret
