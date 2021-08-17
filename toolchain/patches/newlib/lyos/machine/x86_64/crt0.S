#    Lyos CRT0
#
#    This file is part of Lyos.
#
#    Lyos is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    Lyos is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

.global _start
.extern	main
.extern	_exit

.section .text

_start:
    push	%rcx	# environ
    push    %rbx	# argv
    push	%rax	# argc

    mov     %rcx, %rdi
    call	__lyos_init

    pop     %rdi
    pop     %rsi
    pop     %rdx

    push    %rdx
    push    %rsi
    push    %rdi
    call    __set_startup_data

    pop     %rdi
    pop     %rsi
    pop     %rdx
    call	main

    mov     %rax, %rdi
    call	_exit

    hlt
