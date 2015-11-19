/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

.equ P_STACKBASE,	  0
.equ R0REG,			  0
.equ R1REG,		      4
.equ R2REG,		      8
.equ R3REG,		      12
.equ R4REG,		      16
.equ R5REG,		      20
.equ R6REG,		      24
.equ R7REG,		      28
.equ R8REG,		      32
.equ R9REG,		      36
.equ R10REG,		  40
.equ R11REG,		  44
.equ R12REG,		  48
.equ SPREG,		      52
.equ LRREG,			  56
.equ PCREG,			  60
.equ PSRREG,		  64

.equ K_STACK_SIZE,	4096

.equ PSR_MODE,       0x0000001f 
.equ PSR_I,          (1 << 7)
.equ PSR_F,          (1 << 6)
.equ PSR_USR32_MODE, 0x00000010
.equ PSR_SVC32_MODE, 0x00000013

.macro switch_to_svc lr_offset
    sub lr, lr, #\lr_offset
    srsdb   sp!, #PSR_SVC32_MODE
    cps #PSR_SVC32_MODE
.endm

.macro test_in_kernel label
    push    {r3}
    ldr r3, [sp, #8]
    orr r3, r3, #(PSR_F | PSR_I)
    str r3, [sp, #8]
    and r3, r3, #PSR_MODE
    cmp r3, #PSR_USR32_MODE
    pop {r3}
    bne \label
.endm

.macro save_context
    add sp, sp, #8
    ldr lr, [sp]
    stm lr, {r0-r14}^
    ldr r12, [sp, #-8]
    str r12, [lr, #PCREG]
    ldr r12, [sp, #-4]
    str r12, [lr, #PSRREG]
.endm
