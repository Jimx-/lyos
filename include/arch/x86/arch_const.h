/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ARCH_CONST_H_
#define _ARCH_CONST_H_

#define X86_STACK_TOP_RESERVED  (2 * sizeof(reg_t))
#define K_STACK_SIZE    PG_SIZE
extern void * k_stacks;
extern u32 k_stacks_start, k_stacks_end;
#define get_k_stack_top(cpu)    ((void *)(((char*)(k_stacks)) \
                    + 2 * ((cpu) + 1) * K_STACK_SIZE))

/* kernel trap style */
#define KTS_NONE        0
#define KTS_INT         1
#define KTS_SYSENTER    2
#define KTS_SYSCALL     3

/* syscall style */
#define SST_INTEL_SYSENTER  1
#define SST_AMD_SYSCALL     2

/* MSRs */
#define INTEL_MSR_SYSENTER_CS         0x174
#define INTEL_MSR_SYSENTER_ESP        0x175
#define INTEL_MSR_SYSENTER_EIP        0x176

#define AMD_EFER_SCE        (1L << 0)   /* SYSCALL/SYSRET enabled */
#define AMD_MSR_EFER        0xC0000080  /* extended features msr */
#define AMD_MSR_STAR        0xC0000081  /* SYSCALL params msr */

#define NR_IRQS_LEGACY  16
#if CONFIG_X86_IO_APIC
#define NR_IRQ_VECTORS        64
#else
#define NR_IRQ_VECTORS        NR_IRQS_LEGACY
#endif

#endif
