#include <asm/page.h>
#include <asm/const.h>
#include <asm/csr.h>
#include <asm/asm.h>
#include <sconst.h>

    .section ".head","ax"

.globl _lyos_start	/* entry point */

_lyos_start:
    /* mask interrupts */
    csrw sie, zero

.option push
.option norelax
    /* load global pointer */
    la gp, __global_pointer$
.option pop

    /* pick one hart to boot the others */
    la a3, hart_counter
    li a2, 1
    amoadd.w a3, a2, (a3)
    bnez a3, .Lsecondary_start

    /* save hart ID and DTB from bootloader */
    mv s0, a0
    mv s1, a1

    /* setup paging */
    la sp, KStackTop
    call setup_paging
    call enable_paging

    /* reload stack pointer and save hart id */
    la sp, KStackTop
    la tp, init_stackframe
    sw x0, P_CPU(tp)

    /* start kernel */
    mv a0, s0
    mv a1, s1
    call cstart
    tail kernel_main

enable_paging:
    /* relocate return address */
    li a1, KERNEL_VMA
    la a0, _lyos_start
    sub a1, a1, a0
    add ra, ra, a1

    la a0, 1f
    add a0, a0, a1
    csrw stvec, a0

    /* computer satp for initial pgd */
    la a2, initial_pgd
    srl a2, a2, ARCH_PG_SHIFT
    li a1, SATP_MODE
    or a2, a2, a1

    la a0, trampoline_pgd
    srl a0, a0, ARCH_PG_SHIFT
    or a0, a0, a1
    sfence.vma
    csrw sptbr, a0
.align 2
1:
    la a1, .Lno_smp
    csrw stvec, a1

.option push
.option norelax
    /* reload global pointer */
    la gp, __global_pointer$
.option pop

    /* load initial pgd */
    csrw sptbr, a2

    ret

.Lsecondary_start:
#ifdef CONFIG_SMP
    li a1, CONFIG_SMP_MAX_CPUS
    bgeu a0, a1, .Lno_smp

    la a3, .Lno_smp
    csrw stvec, a3

    slli a3, a0, REG_SHIFT
    la a1, __cpu_stack_pointer
    la a2, __cpu_task_pointer
    add a1, a1, a3
    add a2, a2, a3

.Lap_wait:
    REG_L sp, (a1)
    REG_L tp, (a2)
    beqz sp, .Lap_wait
    beqz tp, .Lap_wait

    fence

    call enable_paging
    tail smp_boot_ap
#endif

.align 2
.Lno_smp:
    wfi
    j .Lno_smp

.section .data
KStackSpace:
    .zero K_STACK_SIZE
KStackTop:

.globl k_stacks_start, k_stacks_end
.balign	K_STACK_SIZE
k_stacks_start:
    .space 2 * K_STACK_SIZE * CONFIG_SMP_MAX_CPUS
k_stacks_end:
