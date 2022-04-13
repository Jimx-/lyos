    .section .text

    .extern resumecontext

    .global getcontext
    .global setcontext
    .global ctx_wrapper

getcontext:
    ret

setcontext:
    ret

ctx_wrapper:
    ret
