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

global _cpuid

; void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);

_cpuid:
	push ebp
	push ebx

	mov ebp, [esp + 12]
	mov eax, [ebp]
	mov ebp, [esp + 16]
	mov ebx, [ebp]
	mov ebp, [esp + 20]
	mov ecx, [ebp]
	mov ebp, [esp + 24]
	mov edx, [ebp]

db 0x0F, 0xA2

	mov ebp, [esp + 12]
	mov [ebp], eax
	mov ebp, [esp + 16]
	mov [ebp], ebx
	mov ebp, [esp + 20]
	mov [ebp], ecx
	mov ebp, [esp + 24]
	mov [ebp], edx

	pop ebx
	pop ebp

	ret
