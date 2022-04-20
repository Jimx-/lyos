    #include <asm/protect.h>
    #include <asm/const.h>
    #include <asm/sconst_64.h>

    .section .text

    .macro HWINT label, number
    .global hwint\label
    hwint\label:
    cmpl $SELECTOR_KERNEL_CS, 8(%rsp)
    je 1f

    swapgs
    SAVE_CONTEXT 0, KTS_INT

    mov %rsi, %rdi
    call stop_context

    mov $\number, %rdi
    call generic_handle_irq

    jmp switch_to_user

1:
    push %rbp
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rsi
    push %rdi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    call idle_stop
    mov $\number, %rdi
    call generic_handle_irq

    CLEAR_IF 17*8(%rsp)

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rdi
    pop %rsi
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %rbp

    iretq
    .endm

    HWINT 00, 0
    HWINT 01, 1
    HWINT 02, 2
    HWINT 03, 3
    HWINT 04, 4
    HWINT 05, 5
    HWINT 06, 6
    HWINT 07, 7
    HWINT 08, 8
    HWINT 09, 9
    HWINT 10, 10
    HWINT 11, 11
    HWINT 12, 12
    HWINT 13, 13
    HWINT 14, 14
    HWINT 15, 15

    .macro MAKE_EXCEPTION_HANDLER name, errcode, number
    .global \name
    \name:
    .if \errcode == 0
    pushq $0
    .endif

    pushq $\number
    jmp exception
    .endm

    MAKE_EXCEPTION_HANDLER divide_error, 0, 0
    MAKE_EXCEPTION_HANDLER single_step_exception, 0, 1
    MAKE_EXCEPTION_HANDLER nmi, 0, 2
    MAKE_EXCEPTION_HANDLER breakpoint_exception, 0, 3
    MAKE_EXCEPTION_HANDLER overflow, 0, 4
    MAKE_EXCEPTION_HANDLER bounds_check, 0, 5
    MAKE_EXCEPTION_HANDLER inval_opcode, 0, 6
    MAKE_EXCEPTION_HANDLER copr_not_available, 0, 7
    MAKE_EXCEPTION_HANDLER double_fault, 1, 8
    MAKE_EXCEPTION_HANDLER copr_seg_overrun, 0, 9
    MAKE_EXCEPTION_HANDLER inval_tss, 1, 10
    MAKE_EXCEPTION_HANDLER segment_not_present, 1, 11
    MAKE_EXCEPTION_HANDLER stack_exception, 1, 12
    MAKE_EXCEPTION_HANDLER general_protection, 1, 13
    MAKE_EXCEPTION_HANDLER page_fault, 1, 14
    MAKE_EXCEPTION_HANDLER copr_error, 0, 15

exception:
    cmpl $SELECTOR_KERNEL_CS, 24(%rsp)
    je exception_in_kernel

    swapgs
    SAVE_CONTEXT 16, KTS_INT

    mov %rsi, %rdi
    call stop_context

    mov $0, %rdi
    mov %rsp, %rsi
    call exception_handler

    jmp switch_to_user

exception_in_kernel:
    push %rbp
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rsi
    push %rdi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    mov $1, %rdi
    mov %rsp, %rsi
    add $120, %rsi              // 15 registers
    call exception_handler

    CLEAR_IF 19*8(%rsp)

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rdi
    pop %rsi
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    pop %rbp

    add $16, %rsp
    iretq

    .global sys_call
sys_call:
    swapgs

    SAVE_CONTEXT 0, KTS_INT

    push %rsi // proc ptr
    push %rax

    mov %rsi, %rdi
    call stop_context

    pop %rdi
    mov %rbx, %rsi
    mov (%rsp), %rdx
    call handle_sys_call

    pop %rsi
    mov %rax, RAXREG(%rsi)

    jmp switch_to_user

    .global sys_call_syscall
sys_call_syscall:
    swapgs
    mov %gs:percpu_kstack, %rdx
    mov (%rdx), %rbp            // proc ptr

    mov %rcx, RIPREG(%rbp)
    mov %rsp, RSPREG(%rbp)
    mov %rax, P_ORIGRAX(%rbp)
    mov %rbx, RBXREG(%rbp)
    mov %r11, RFLAGSREG(%rbp)
    movl $KTS_SYSCALL, P_TRAPSTYLE(%rbp)

    mov %rdx, %rsp
    push %rax

    mov %rbp, %rdi
    call stop_context

    pop %rdi
    mov %rbx, %rsi
    mov %rbp, %rdx
    call handle_sys_call

    mov %rax, RAXREG(%rbp)
    jmp switch_to_user

    .global restore_user_context_int
restore_user_context_int:
    mov %rdi, %rsp
    POP_REGS

    add $8, %rsp // Skip orig_ax and we are at the IRET frame.

    swapgs
    iretq

    .global restore_user_context_sysenter
restore_user_context_sysenter:
    retq

    .global restore_user_context_syscall
restore_user_context_syscall:
    mov RFLAGSREG(%rdi), %r11
    mov RAXREG(%rdi), %rax
    mov RIPREG(%rdi), %rcx
    mov RSPREG(%rdi), %rsp

    swapgs
    sysretq
