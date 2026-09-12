#   This file is part of Lyos.
#
#   Lyos is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   Lyos is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

/*
 * 32-bit protected mode guest payloads, flat segments, paging disabled.
 * The section is copied into guest RAM at GPA 0x1000 by the VMM.
 *
 * Guest memory layout used by the payloads:
 *   0x2000  marker word
 *   0x2004  emulated port input result (io-in payload)
 *   0x2100  SSE pattern A (xmm0) / 0x2110 pattern A (xmm1)
 *   0x2120  xmm0 store target / 0x2130 xmm1 store target
 *   0x2200  CPUID leaf 1 EAX / 0x2204 CPUID leaf 1 EDX
 */

    .code32

    .section .guest32, "ax"

    .globl __guest32_start
__guest32_start:

/* entry/HLT/resume */
    .globl vmx_guest_hlt
vmx_guest_hlt:
    movl $0x600d0001, 0x2000
    hlt
    movl $0x600d0002, 0x2000
    hlt
    jmp .

/* intercepted port I/O to the debug console */
    .globl vmx_guest_io
vmx_guest_io:
    mov $0x42, %al
    mov $0xe9, %dx
    out %al, %dx
    hlt
    jmp .

/* input from an unattached port is emulated as all ones */
    .globl vmx_guest_in
vmx_guest_in:
    xor %eax, %eax
    mov $0x2f8, %dx
    in %dx, %al
    mov %eax, 0x2004
    hlt
    jmp .

/* CPUID is emulated with the constrained x87/SSE model */
    .globl vmx_guest_cpuid
vmx_guest_cpuid:
    mov $1, %eax
    xor %ecx, %ecx
    cpuid
    mov %eax, 0x2200
    mov %edx, 0x2204
    hlt
    jmp .

/* access to an unbacked GPA */
    .globl vmx_guest_ept
vmx_guest_ept:
    xor %eax, %eax
    .globl vmx_guest_ept_fault
vmx_guest_ept_fault:
    mov 0xf0000000, %eax
    hlt
    jmp .

/* SSE register use across exits */
    .globl vmx_guest_fpu
vmx_guest_fpu:
    movdqu 0x2100, %xmm0
    movdqu 0x2110, %xmm1
    hlt
    movdqu %xmm0, 0x2120
    movdqu %xmm1, 0x2130
    hlt
    jmp .

    .globl __guest32_end
__guest32_end:

    .section .note.GNU-stack, "", @progbits
