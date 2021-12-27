.section .text

.global __clone

.equ STACK_PARENT,	1
.equ NR_SENDREC,	1

__clone:
    push %rbp
    mov %rsp, %rbp

    push %rbx
    push $STACK_PARENT
    movl $NR_SENDREC, %eax
    mov %rdi, %rbx
    int $0x90

    lock; add $0,0(%rsp)

    mov (%rsp), %rcx
    cmpq $STACK_PARENT, %rcx
    je __clone_parent

    add $8, %rsp
    pop %rax
    pop %rdi
    jmp *%rax

    call _exit

__clone_parent:
    add $8, %rsp
    pop %rbx
    pop %rbp
    ret
