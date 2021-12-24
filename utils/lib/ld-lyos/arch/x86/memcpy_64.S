    .global memcpy

memcpy:
    movq %rdi, %rax
    movq %rdx, %rcx
    shrq $3, %rcx
    andl $7, %edx
    rep movsq
    movl %edx, %ecx
    rep movsb
    ret
