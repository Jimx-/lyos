    #include <lyos/config.h>
    #include <asm/const.h>
    #include <asm/protect.h>
    #include <asm/page.h>

    .extern StackTop
    .extern smp_boot_ap

    .section .text

    .balign 0x1000
    .global trampoline
    .code16
trampoline:
    cli

    mov %cs, %ax
    mov %ax, %ds

    lgdt __ap_gdt - trampoline
    lidt __ap_idt - trampoline

    movl __ap_pgd - trampoline, %eax
    movl %eax, %cr3

    /* Enable protected mode */
    mov %cr0, %eax
    orb $1, %al
    mov %eax, %cr0

    ljmpl $SELECTOR_KERNEL32_CS, $startup_ap_32 - KERNEL_VMA

    .balign 4
    .code32
startup_ap_32:
    mov $SELECTOR_KERNEL_DS, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Enable PAE */
    mov %cr4, %eax
    or $I386_CR4_PAE, %eax
    mov %eax, %cr4

    /* Enable long mode */
    mov $AMD_MSR_EFER, %ecx
    rdmsr
    or $AMD_EFER_LME, %eax
    wrmsr

    /* Enable paging */
    mov %cr0, %eax
    or $I386_CR0_PG, %eax
    mov %eax, %cr0

    ljmp $SELECTOR_KERNEL_CS, $startup_ap_64 - KERNEL_VMA

    .code64
    .balign 8
startup_ap_64:
    mov $StackTop - 16, %rsp

    leaq smp_boot_ap, %rax
    pushq %rax
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
    .space 16

    .balign 4
    .global __ap_idt
__ap_idt:
    .space 16

    .balign 4
    .global __ap_gdt_table
__ap_gdt_table:
    .space 128*16

    .balign 4
    .global __ap_idt_table
__ap_idt_table:
    .space 256*16

    .balign 0x1000
    .global __trampoline_end
__trampoline_end:
