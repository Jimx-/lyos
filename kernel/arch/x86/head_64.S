    #include <asm/const.h>
    #include <asm/page.h>

    .section .multiboot

    .set MODULEALIGN, 1<<0                    // align loaded modules on page boundaries
    .set MEMINFO,     1<<1                    // provide memory map
    .set FLAGS,       MODULEALIGN | MEMINFO   // this is the Multiboot 'flag' field
    .set MAGIC,       0x1BADB002              // 'magic number' lets bootloader find the header
    .set CHECKSUM,    -(MAGIC + FLAGS)        // checksum required

    .align 8
multiboot_header:
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    .long 32

    .section ".head.text","ax",@progbits
    .code32
    .align 4

    .global _start
    .type _start, @function
_start:
    mov $StackTop - KERNEL_VMA, %esp

    /* Save Multiboot parameters */
    pushl $0
    pushl %eax
    pushl $0
    pushl %ebx

    mov $init_top_pgt - KERNEL_VMA, %eax
    mov %eax, %cr3

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

    lgdt gdtr - KERNEL_VMA
    ljmp $0x08, $long_mode_enabled - KERNEL_VMA

    .code64
    .align 8
    .section .text

long_mode_enabled:
    cli
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    leaq startup_64, %rax
    pushq %rax
    retq

    .extern cstart
    .extern kernel_main

startup_64:
    add $KERNEL_VMA, %rsp

    pop %rdi
    pop %rsi

    callq cstart

    jmp kernel_main

    hlt

    .section .data
/* Automate the creation of 1 to 1 mapping pmd entries */
#define PMDS(START, PERM, COUNT)			\
    i = 0 ;						\
    .rept (COUNT) ;					\
    .quad	(START) + (i << ARCH_PMD_SHIFT) + (PERM) ;	\
    i = i + 1 ;					\
    .endr

    .align 0x1000
init_top_pgt:
    .quad level3_ident_pgt - KERNEL_VMA + 0x07 /* PG_PRESENT | PG_RW | PG_USER */
    .fill 510,8,0
    .quad level3_kernel_pgt - KERNEL_VMA + 0x07 /* PG_PRESENT | PG_RW | PG_USER */

level3_ident_pgt:
    .quad level2_ident_pgt - KERNEL_VMA + 0x07 /* PG_PRESENT | PG_RW | PG_USER */
    .fill 511,8,0

level2_ident_pgt:
    /* Identity map the first 1G. */
    PMDS(0, 0x87 /* PG_PRESENT | PG_RW | PG_USER | PG_BIGPAGE */, ARCH_VM_PMD_ENTRIES)

level3_kernel_pgt:
    .fill 510,8,0
    .quad level2_ident_pgt - KERNEL_VMA + 0x07
    .quad 0

    .align 8
gdtr:
    .word gdt_end - gdt_base
    .quad gdt_base - KERNEL_VMA

gdt_base:
    /* Null */
    .quad 0
    /* Code */
    .word 0
    .word 0
    .byte 0
    .byte 0x9a
    .byte 0x20
    .byte 0
    /*  Data */
    .word 0xffff
    .word 0
    .byte 0
    .byte 0x92
    .byte 0
    .byte 0
gdt_end:

    .section .bss
    .align 0x1000
StackSpace:
    .skip K_STACK_SIZE
StackTop:
