    .global memset

memset:
    movq %rdi,%r9
    movq %rdx,%rcx
    andl $7,%edx
    shrq $3,%rcx
    movzbl %sil,%esi
    movabs $0x0101010101010101,%rax
    imulq %rsi,%rax
    rep stosq
    movl %edx,%ecx
    rep stosb
    movq %r9,%rax
    ret
