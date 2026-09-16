/* AMD SVM VMRUN entry/exit glue. */
#include <asm/const.h>

#define VCPU_GPRS     0
#define VCPU_VMCB     0x80
#define VCPU_EXIT_RSP 0x88

    .section .text
    .globl svm_enter_guest
    .type svm_enter_guest, @function
svm_enter_guest:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, VCPU_EXIT_RSP(%rdi)
    mov %rdi, svm_current_vcpu(%rip)

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
    mov VCPU_VMCB(%rdi), %rax
    mov VCPU_GPRS+56(%rdi), %rdi

    clgi
    vmrun %rax

    push %r10
    push %r11
    mov svm_current_vcpu(%rip), %r11
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
    pop %rax
    mov %rax, VCPU_GPRS+88(%r11)
    pop %rax
    mov %rax, VCPU_GPRS+80(%r11)
    mov VCPU_EXIT_RSP(%r11), %rsp
    stgi

    xor %eax, %eax
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
