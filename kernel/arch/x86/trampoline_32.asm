%include "sconst.inc"

global trampoline
global __ap_id
global __ap_pgd
global __ap_gdt
global __ap_idt
global __ap_gdt_table
global __ap_idt_table
global __trampoline_end

extern StackTop
extern smp_boot_ap

bits 16

[section .text]

ALIGN 0x1000

trampoline:
    cli

    mov ax, cs
    mov ds, ax

    lgdt [__ap_gdt - trampoline]
    lidt [__ap_idt - trampoline]

    ; switch to protected mode
    mov eax, cr0
    or al, 1
    mov cr0, eax

    mov eax, cr4
    or  eax, I386_CR4_PSE	; enable big page
    mov cr4, eax

    ; set cr3
    mov eax, [__ap_pgd - trampoline]
    mov cr3, eax

    ; enable paging
    mov	eax, cr0
    or	eax, I386_CR0_PG
    or  eax, I386_CR0_WP
    mov	cr0, eax

    mov eax, cr4
    or  eax, I386_CR4_PGE	; enable big page
    mov cr4, eax

    jmp dword SELECTOR_KERNEL_CS:trampoline_32

    bits 32
    ALIGN 4
trampoline_32:
    mov ax, SELECTOR_KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, StackTop - X86_STACK_TOP_RESERVED

    jmp smp_boot_ap
    hlt

ALIGN 4
__ap_id:
    dd	0
__ap_pgd:
    dd      0
__ap_gdt:
    dq	0
__ap_idt:
    dq      0
__ap_gdt_table:
    times 128*8 db 0
__ap_idt_table:
    times 256*8 db 0
ALIGN 0x1000
__trampoline_end:
