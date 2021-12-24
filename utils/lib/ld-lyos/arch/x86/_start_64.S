    .section .text

    .align 8
    .globl _start
    .hidden _start
    .type _start,@function
_start:
    push %rcx // environ
    push %rdx // argv
    push %rax // argc

    call 1f
1:
    pop %rdx
    leaq	_DYNAMIC-1b(%rdx), %rdi
    mov	%rdi, %rsi
    sub	_GLOBAL_OFFSET_TABLE_-1b(%rdx), %rsi

    call ldso_relocate_nonplt_self

    mov 16(%rsp), %rdi
    call __lyos_init

    mov (%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    call ldso_main
    mov %rax, %rbx

    pop %rax
    pop %rdx
    pop %rcx
    call *%rbx

    .align	8
    .globl	ldso_bind_entry
    .hidden	ldso_bind_entry
    .type	ldso_bind_entry,@function
ldso_bind_entry:
    push %rsi
    push %rdi
    mov 16(%rsp), %rdi
    mov 24(%rsp), %rsi

    push %rax
    push %rcx
    push %rdx
    push %r8
    push %r9
    push %r10

    call ldso_bind
    mov %rax, %r11

    pop %r10
    pop %r9
    pop %r8
    pop %rdx
    pop %rcx
    pop %rax

    pop %rdi
    pop %rsi
    add $16, %rsp
    jmp *%r11
