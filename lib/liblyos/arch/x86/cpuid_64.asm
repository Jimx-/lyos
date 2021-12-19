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
    push rbx

    mov r10, rdx
    mov r11, rcx

    mov eax, [rdi]
    mov ebx, [rsi]
    mov ecx, [r10]
    mov edx, [r11]

db 0x0F, 0xA2

    mov [rdi], eax
    mov [rsi], ebx
    mov [r10], ecx
    mov [r11], edx

    pop rbx
    ret
