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

bits 32
[section .text]

global read_tsc

read_tsc:
	push edx
	push eax
db	0x0f
db	0x31
	push ebp
	mov ebp, [esp + 16]
	mov [ebp], edx
	mov ebp, [esp + 20]
	mov [ebp], eax
	pop ebp
	pop eax
	pop edx
	ret
	