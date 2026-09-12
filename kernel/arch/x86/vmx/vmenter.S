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

#include <asm/protect.h>
#include <asm/const.h>
#include <asm/sconst_64.h>

/* offsets into struct vmx_vcpu, checked at compile time in vmx.h */
#define VCPU_GPRS     0
#define VCPU_EXITRSP  0x88
#define VCPU_LAUNCHED 0x2a4

    .section .bss
    .globl vmx_exit_stack
vmx_exit_stack:
    .space 8192
    .globl vmx_exit_stack_top
vmx_exit_stack_top:

    .section .text

###############################################################################
# int vmx_enter_guest(struct vmx_vcpu* vcpu)
#
# Loads guest GPRs and executes VMLAUNCH/VMRESUME.  On a successful VM entry
# the CPU leaves this code; the VM exit re-enters at vmx_exit_entry on the
# dedicated exit stack and unwinds back to the caller of vmx_enter_guest.
###############################################################################
    .globl vmx_enter_guest
    .type vmx_enter_guest, @function
vmx_enter_guest:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    # record the unwind point and the current vcpu for the exit path
    mov %rsp, VCPU_EXITRSP(%rdi)
    mov %rdi, vmx_current_vcpu(%rip)

    # MOV preserves flags, so select launch/resume before replacing host GPRs.
    cmpl $0, VCPU_LAUNCHED(%rdi)

    # load guest GPRs, rdi last
    mov VCPU_GPRS+0(%rdi), %rax
    mov VCPU_GPRS+8(%rdi), %rcx
    mov VCPU_GPRS+16(%rdi), %rdx
    mov VCPU_GPRS+24(%rdi), %rbx
    mov VCPU_GPRS+40(%rdi), %rbp
    mov VCPU_GPRS+48(%rdi), %rsi
    mov VCPU_GPRS+64(%rdi), %r8
    mov VCPU_GPRS+72(%rdi), %r9
    mov VCPU_GPRS+80(%rdi), %r10
    mov VCPU_GPRS+88(%rdi), %r11
    mov VCPU_GPRS+96(%rdi), %r12
    mov VCPU_GPRS+104(%rdi), %r13
    mov VCPU_GPRS+112(%rdi), %r14
    mov VCPU_GPRS+120(%rdi), %r15
    mov VCPU_GPRS+56(%rdi), %rdi

    jne 1f
    vmlaunch
    jmp 2f
1:
    vmresume
2:
    # the launch instruction itself failed (VMfail)
    # -1 = VMfailValid (ZF), -2 = VMfailInvalid (CF).
    setc %al
    movzbl %al, %eax
    neg %eax
    sub $1, %eax
    mov vmx_current_vcpu(%rip), %r10
    mov VCPU_EXITRSP(%r10), %rsp
    jmp 3f

###############################################################################
# VM exit entry point (VMCS HOST_RIP).  Runs on the dedicated exit stack with
# IF=0 and host state loaded from the VMCS host-state area.  All GPRs still
# hold guest values.
###############################################################################
    .globl vmx_exit_entry
    .type vmx_exit_entry, @function
vmx_exit_entry:
    # save guest GPRs; only the stack and vmx_current_vcpu are usable
    push %r10
    push %r11
    mov vmx_current_vcpu(%rip), %r11
    mov %rax, VCPU_GPRS+0(%r11)
    mov %rcx, VCPU_GPRS+8(%r11)
    mov %rdx, VCPU_GPRS+16(%r11)
    mov %rbx, VCPU_GPRS+24(%r11)
    mov %rbp, VCPU_GPRS+40(%r11)
    mov %rsi, VCPU_GPRS+48(%r11)
    mov %rdi, VCPU_GPRS+56(%r11)
    mov %r8, VCPU_GPRS+64(%r11)
    mov %r9, VCPU_GPRS+72(%r11)
    mov %r12, VCPU_GPRS+96(%r11)
    mov %r13, VCPU_GPRS+104(%r11)
    mov %r14, VCPU_GPRS+112(%r11)
    mov %r15, VCPU_GPRS+120(%r11)
    pop %rax                # guest r11
    mov %rax, VCPU_GPRS+88(%r11)
    pop %rax                # guest r10
    mov %rax, VCPU_GPRS+80(%r11)

    # unwind to the launch stack and run the C exit dispatch
    mov VCPU_EXITRSP(%r11), %rsp
    sub $8, %rsp
    mov %r11, %rdi
    call vmx_exit_dispatch
    add $8, %rsp

3:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
