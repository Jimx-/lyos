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

bits 64
[section .text]

global _cpuid

; void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);

_cpuid:
    push rbp
    push rbx

    mov rax, [rdi]
    mov rbx, [rsi]
    mov rcx, [rdx]
    mov rdx, [rcx]

db 0x0F, 0xA2

    mov [rdi], rax
    mov [rsi], rbx
    mov [rdx], rcx
    mov [rcx], rdx

    pop rbx
    pop rbp

    ret
