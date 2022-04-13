    #include <sconst.h>

    .section .text

    .globl vectors
    .globl restore_user_context

    .macro kernel_exit, el
    ldp     x21, x22, [sp, #PCREG]

    .if \el == 0
    ldr x23, [sp, #SPREG]
    msr sp_el0, x23 // Restore EL0 stack pointer.

    // Save LR to kernel stack so that we can restore it later.
    ldr x0, [sp, #P_KERNELSP]
    ldr x1, [sp, #LRREG]
    str x1, [x0, #-8]
    .endif

    msr     elr_el1, x21
    msr     spsr_el1, x22
    ldp     x0, x1, [sp, #16 * 0]
    ldp     x2, x3, [sp, #16 * 1]
    ldp     x4, x5, [sp, #16 * 2]
    ldp     x6, x7, [sp, #16 * 3]
    ldp     x8, x9, [sp, #16 * 4]
    ldp     x10, x11, [sp, #16 * 5]
    ldp     x12, x13, [sp, #16 * 6]
    ldp     x14, x15, [sp, #16 * 7]
    ldp     x16, x17, [sp, #16 * 8]
    ldp     x18, x19, [sp, #16 * 9]
    ldp     x20, x21, [sp, #16 * 10]
    ldp     x22, x23, [sp, #16 * 11]
    ldp     x24, x25, [sp, #16 * 12]
    ldp     x26, x27, [sp, #16 * 13]
    ldp     x28, x29, [sp, #16 * 14]

    .if \el == 0
    ldr lr, [sp, #P_KERNELSP]
    mov sp, lr
    ldr lr, [sp, #-8]
    eret
    .endif
    .endm

    .align	11
vectors:

restore_user_context:
    mov sp, x0  // proc ptr

    kernel_exit 0
