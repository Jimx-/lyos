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

ALIGN 8

INT_VECTOR_SYS_CALL equ 0x90

[SECTION .usermapped]

global syscall_int
global syscall_sysenter
global syscall_syscall

syscall_int:
    push rbp
    mov rbp, rsp

    push rbx
    mov rax, rdi
    mov rbx, rsi
    int INT_VECTOR_SYS_CALL

    pop rbx
    pop rbp

    ret

syscall_sysenter:
    ret

syscall_syscall:
    push rbp
    mov rbp, rsp

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov rax, rdi
    mov rbx, rsi
    syscall

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    pop rbp
    ret
