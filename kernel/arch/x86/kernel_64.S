    .section .text

    .macro HWINT number
    .global hwint\number
    hwint\number:
    retq
    .endm

    HWINT 00
    HWINT 01
    HWINT 02
    HWINT 03
    HWINT 04
    HWINT 05
    HWINT 06
    HWINT 07
    HWINT 08
    HWINT 09
    HWINT 10
    HWINT 11
    HWINT 12
    HWINT 13
    HWINT 14
    HWINT 15

    .macro MAKE_EXCEPTION_HANDLER name, errcode, number
    .global \name
    \name:
    .if \errcode == 0
    push $0
    .endif

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
    retq

    .global sys_call
sys_call:
    retq

    .global sys_call_sysenter
sys_call_sysenter:
    retq

    .macro SYS_CALL_SYSCALL_PER_CPU cpu
    .global sys_call_syscall_cpu\cpu
    sys_call_syscall_cpu\cpu:
    retq
    .endm

    SYS_CALL_SYSCALL_PER_CPU    0
    SYS_CALL_SYSCALL_PER_CPU	1
    SYS_CALL_SYSCALL_PER_CPU	2
    SYS_CALL_SYSCALL_PER_CPU	3
    SYS_CALL_SYSCALL_PER_CPU	4
    SYS_CALL_SYSCALL_PER_CPU	5
    SYS_CALL_SYSCALL_PER_CPU	6
    SYS_CALL_SYSCALL_PER_CPU	7

    .global restore_user_context_int
restore_user_context_int:
    retq

    .global restore_user_context_sysenter
restore_user_context_sysenter:
    retq

    .global restore_user_context_syscall
restore_user_context_syscall:
    retq
