    #include <sconst.h>
    #include <lyos/const.h>
    #include <asm/sysreg.h>
    #include <asm/page.h>
    #include <asm/const.h>

    #define __PHYS_OFFSET _text

    .section	".head.text","ax"

_head:
    b primary_entry
    .long 0
    .quad 0 // Image load offset from start of RAM
    .quad _KERN_SIZE // Effective size of kernel image
    .quad 0x2 // Informative flags
    .quad 0
    .quad 0
    .quad 0
    .ascii "ARM\x64"
    .long 0

    .section .text
primary_entry:
    mov x21, x0

    adr_l   x0, boot_args
    stp     x21, x1, [x0]
    stp     x2, x3, [x0, #16]

    bl  setup_el
    bl  __create_page_tables

    bl  __cpu_setup
    b   __primary_switch

    .macro  create_table_entry, tbl, virt, shift, ptrs, tmp1, tmp2
    lsr     \tmp1, \virt, #\shift
    and     \tmp1, \tmp1, #\ptrs - 1
    add     \tmp2, \tbl, #ARCH_PG_SIZE
    orr     \tmp2, \tmp2, #_ARM64_PMD_TYPE_TABLE
    str     \tmp2, [\tbl, \tmp1, lsl #3]
    add     \tbl, \tbl, #ARCH_PG_SIZE
    .endm

    .macro	create_pgd_entry, tbl, virt, tmp1, tmp2
    create_table_entry \tbl, \virt, ARCH_PGD_SHIFT, ARCH_VM_DIR_ENTRIES, \tmp1, \tmp2
#if INIT_PGTABLE_LEVELS > 3
    create_table_entry \tbl, \virt, ARCH_PUD_SHIFT, ARCH_VM_PUD_ENTRIES, \tmp1, \tmp2
#endif
#if INIT_PGTABLE_LEVELS > 2
    create_table_entry \tbl, \virt, SWAPPER_TABLE_SHIFT, ARCH_VM_PT_ENTRIES, \tmp1, \tmp2
#endif
    .endm

    .macro  create_block_map, tbl, flags, phys, start, end
    lsr     \phys, \phys, #SWAPPER_BLOCK_SHIFT
    lsr     \start, \start, #SWAPPER_BLOCK_SHIFT
    and     \start, \start, #ARCH_VM_PT_ENTRIES - 1
    orr     \phys, \flags, \phys, lsl #SWAPPER_BLOCK_SHIFT
    lsr     \end, \end, #SWAPPER_BLOCK_SHIFT
    and     \end, \end, #ARCH_VM_PT_ENTRIES - 1
1:  str     \phys, [\tbl, \start, lsl #3]
    add     \start, \start, #1
    add     \phys, \phys, #SWAPPER_BLOCK_SIZE
    cmp     \start, \end
    b.ls    1b
    .endm

__create_page_tables:
    mov     x28, lr

    adrp    x0, idmap_pg_dir
    ldr     x1, =(IDMAP_DIR_SIZE + INIT_DIR_SIZE)
1:  stp     xzr, xzr, [x0], #16
    stp     xzr, xzr, [x0], #16
    stp     xzr, xzr, [x0], #16
    stp     xzr, xzr, [x0], #16
    subs    x1, x1, #64
    b.ne    1b

    mov x7, (_ARM64_PMD_TYPE_SECT|_ARM64_SECT_S|_ARM64_SECT_AF)

    adrp    x0, idmap_pg_dir
    adrp    x3, __idmap_text_start

    create_pgd_entry x0, x3, x5, x6
    mov     x5, x3
    adr_l   x6, __idmap_text_end
    create_block_map x0, x7, x3, x5, x6

    adrp    x0, init_pg_dir
    mov_q   x5, KERNEL_VMA
    create_pgd_entry x0, x5, x3, x6
    adrp     x6, _end
    adrp     x3, _text
    sub      x6, x6, x3
    add      x6, x6, x5
    create_block_map x0, x7, x3, x5, x6

    ret x28

    .section    ".idmap.text","ax"
setup_el:
    mrs x0, CurrentEL
    cmp x0, #CurrentEL_EL2
    b.eq init_el2

init_el1:
    wfe
    b init_el1

init_el2:
    mov_q   x0, SCTLR_EL1_RES1
    msr     sctlr_el1, x0

    mov     x0, xzr
    mov_q   x0, (BIT(31)|BIT(1)) // HCR.RW | HCR.SWIO
    msr     hcr_el2, x0

    mov     x0, #(PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE_EL1h)
    msr     spsr_el2, x0
    msr     elr_el2, lr
    eret

__primary_switch:
    bl __enable_paging

    ldr  x8, =__primary_switched
    adrp x0, __PHYS_OFFSET
    br   x8

    .macro init_cpu_task tsk, tmp1, tmp2
    msr sp_el0, \tsk

    adr_l \tmp1, __cpulocals_offset
    ldr w\tmp2, [\tsk, #P_CPU]
    ldr \tmp1, [\tmp1, \tmp2, lsl #3]
    msr tpidr_el1, \tmp1
    .endm

__primary_switched:
    adr_l   x5, KStackTop
    mov     sp, x5

    adr_l x4, init_stackframe
    init_cpu_task x4, x5, x6

    adr_l   x8, vectors
    msr     vbar_el1, x8
    isb

    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    ldr_l   x4, kimage_vaddr
    sub     x4, x4, x0
    str_l   x4, kimage_voffset, x5

    adr_l   x0, _bss
    mov     w1, wzr
    adr_l   x2, _ebss
    sub     x2, x2, x0
    bl  memset
    dsb ishst

    str_l   x21, __fdt_pointer, x5

    mov x0, x21
    bl cstart

    ldp x29, x30, [sp], #16
    bl kernel_main

__cpu_setup:
    tlbi    vmalle1
    dsb     nsh

    mov x1, #3 << 20
    msr cpacr_el1, x1
    mov x1, #1 << 12
    msr mdscr_el1, x1
    isb

    mair    .req    x17
    tcr     .req    x16

    mov_q   mair, MAIR_EL1_SET
    mov_q   tcr, TCR_TxSZ(VA_BITS) | TCR_CACHE_FLAGS | TCR_SMP_FLAGS | \
            TCR_TG_FLAGS | TCR_ASID16 | TCR_TBI0 | TCR_A1

    mrs     x5, ID_AA64MMFR0_EL1
    ubfx    x5, x5, #ID_AA64MMFR0_PARANGE_SHIFT, #3
    mov     x6, #ID_AA64MMFR0_PARANGE_MAX
    cmp     x5, x6
    csel    x5, x6, x5, hi
    bfi     tcr, x5, #TCR_IPS_SHIFT, #3

    msr     mair_el1, mair
    msr     tcr_el1, tcr

    mov_q   x0, INIT_SCTLR_EL1_MMU_ON
    ret

    .unreq  mair
    .unreq  tcr

__enable_paging:
    adrp    x1, idmap_pg_dir
    adrp    x2, init_pg_dir
    msr     ttbr0_el1, x1
    msr     ttbr1_el1, x2
    isb
    msr     sctlr_el1, x0
    isb
    ic  iallu
    dsb nsh
    isb
    ret

    .pushsection ".rodata", "a"
kimage_vaddr:
    .quad   _text

    .globl kimage_voffset
kimage_voffset:
    .quad   _text
    .popsection

    .section .data
KStackSpace:
    .zero K_STACK_SIZE
KStackTop:
