    .section .text

    .macro apic_hwint number
    .global apic_hwint\number
    .align 16
    apic_hwint\number:
    retq
    .endm

    apic_hwint 00
    apic_hwint 01
    apic_hwint 02
    apic_hwint 03
    apic_hwint 04
    apic_hwint 05
    apic_hwint 06
    apic_hwint 07
    apic_hwint 08
    apic_hwint 09
    apic_hwint 10
    apic_hwint 11
    apic_hwint 12
    apic_hwint 13
    apic_hwint 14
    apic_hwint 15
    apic_hwint 16
    apic_hwint 17
    apic_hwint 18
    apic_hwint 19
    apic_hwint 20
    apic_hwint 21
    apic_hwint 22
    apic_hwint 23
    apic_hwint 24
    apic_hwint 25
    apic_hwint 26
    apic_hwint 27
    apic_hwint 28
    apic_hwint 29
    apic_hwint 30
    apic_hwint 31
    apic_hwint 32
    apic_hwint 33
    apic_hwint 34
    apic_hwint 35
    apic_hwint 36
    apic_hwint 37
    apic_hwint 38
    apic_hwint 39
    apic_hwint 40
    apic_hwint 41
    apic_hwint 42
    apic_hwint 43
    apic_hwint 44
    apic_hwint 45
    apic_hwint 46
    apic_hwint 47
    apic_hwint 48
    apic_hwint 49
    apic_hwint 50
    apic_hwint 51
    apic_hwint 52
    apic_hwint 53
    apic_hwint 54
    apic_hwint 55
    apic_hwint 56
    apic_hwint 57
    apic_hwint 58
    apic_hwint 59
    apic_hwint 60
    apic_hwint 61
    apic_hwint 62
    apic_hwint 63

    .global apic_spurious_intr
    .align 16
apic_spurious_intr:
    retq

    .global apic_error_intr
    .align 16
apic_error_intr:
    retq

    .global apic_timer_int_handler
    .align 16
apic_timer_int_handler:
    retq