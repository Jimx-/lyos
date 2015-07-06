.global memset

.section .text

memset:
    push    %edi
    movl    16(%esp), %ecx
    movl    12(%esp), %eax
    mov     8(%esp), %edi
    rep     stosb

    movl    8(%esp), %eax
    pop     %edi
    ret
