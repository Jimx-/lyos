    .section .text

    .balign 0x1000

    .global trampoline
trampoline:
    retq

    .balign 4
    .global __ap_id
__ap_id:
    .space 4

    .balign 4
    .global __ap_pgd
__ap_pgd:
    .space 4

    .balign 4
    .global __ap_gdt
__ap_gdt:
    .space 8

    .balign 4
    .global __ap_idt
__ap_idt:
    .space 8

    .balign 4
    .global __ap_gdt_table
__ap_gdt_table:
    .space 128

    .balign 4
    .global __ap_idt_table
__ap_idt_table:
    .space 256

    .balign 0x1000
    .global __trampoline_end
__trampoline_end:
