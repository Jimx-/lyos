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

/* number of entries in GDT and IDT */
#define IDT_SIZE 256

#define X86_STACK_TOP_RESERVED (2 * sizeof(reg_t))
#define K_STACK_SIZE           ARCH_PG_SIZE

#ifndef __ASSEMBLY__

extern void* k_stacks;
extern u32 k_stacks_start, k_stacks_end;
#define get_k_stack_top(cpu) \
    ((void*)(((char*)(k_stacks)) + 2 * ((cpu) + 1) * K_STACK_SIZE))

#endif

/* kernel trap style */
#define KTS_NONE     0
#define KTS_INT      1
#define KTS_SYSENTER 2
#define KTS_SYSCALL  3

/* syscall style */
#define SST_INTEL_SYSENTER 1
#define SST_AMD_SYSCALL    2

/* MSRs */
#define INTEL_MSR_PERF_PMC0            0xc1
#define INTEL_MSR_SYSENTER_CS          0x174
#define INTEL_MSR_SYSENTER_ESP         0x175
#define INTEL_MSR_SYSENTER_EIP         0x176
#define INTEL_MSR_PERF_GLOBAL_CTRL     0x38F
#define INTEL_MSR_PERF_GLOBAL_STATUS   0x38E
#define INTEL_MSR_PERF_FIXED_CTRL      0x38D
#define INTEL_MSR_PERF_FIXED_CTR0      0x309
#define INTEL_MSR_PERF_GLOBAL_OVF_CTRL 0x390

#define INTEL_MSR_ERFEVTSEL_UNHALTED_CORE_CYCLES (0x3c)
#define INTEL_MSR_ERFEVTSEL_USR                  (1 << 16)
#define INTEL_MSR_ERFEVTSEL_OS                   (1 << 17)
#define INTEL_MSR_ERFEVTSEL_INT_EN               (1 << 20)
#define INTEL_MSR_ERFEVTSEL_EN                   (1 << 22)

#define AMD_EFER_SCE           (1 << 0)   /* SYSCALL/SYSRET enabled */
#define AMD_EFER_LME           (1 << 8)   /* Long mode enabled */
#define AMD_MSR_EFER           0xC0000080 /* extended features msr */
#define AMD_MSR_STAR           0xC0000081 /* SYSCALL params msr */
#define AMD_MSR_LSTAR          0xC0000082 /* long mode SYSCALL target msr */
#define AMD_MSR_SYSCALL_MASK   0xC0000084 /* EFLAGS mask for syscall */
#define AMD_MSR_FS_BASE        0xC0000100 /* 64-bit FS base */
#define AMD_MSR_GS_BASE        0xC0000101 /* 64-bit GS base */
#define AMD_MSR_KERNEL_GS_BASE 0xC0000102 /* SwapGS GS shadow */

#define NR_IRQS_LEGACY 16
#if CONFIG_X86_IO_APIC
#define NR_IRQ_VECTORS 64
#else
#define NR_IRQ_VECTORS NR_IRQS_LEGACY
#endif

#define INTEL_CPUID_EBX 0x756e6547 /* ASCII value of "Genu" */
#define INTEL_CPUID_EDX 0x49656e69 /* ASCII value of "ineI" */
#define INTEL_CPUID_ECX 0x6c65746e /* ASCII value of "ntel" */

#define AMD_CPUID_EBX 0x68747541 /* ASCII value of "Auth" */
#define AMD_CPUID_EDX 0x69746e65 /* ASCII value of "enti" */
#define AMD_CPUID_ECX 0x444d4163 /* ASCII value of "cAMD" */

#define CPU_VENDOR_INTEL   1
#define CPU_VENDOR_AMD     2
#define CPU_VENDOR_UNKNOWN 0xff

/* 8253/8254 PIT (Programmable Interval Timer) */
#define TIMER0     0x40 /* I/O port for timer channel 0 */
#define TIMER2     0x42 /* I/O port for timer channel 2 */
#define TIMER_MODE 0x43 /* I/O port for timer mode control */
#define RATE_GENERATOR                                                           \
    0x34                    /* 00-11-010-0 :                                     \
                             * Counter0 - LSB then MSB - rate generator - binary \
                             */
#define TIMER_FREQ 1193182L /* clock frequency for timer in PC and AT */

/* VGA */
#define CRTC_ADDR_REG 0x3D4 /* CRT Controller Registers - Addr Register */
#define CRTC_DATA_REG 0x3D5 /* CRT Controller Registers - Data Register */
#define START_ADDR_H  0xC   /* reg index of video mem start addr (MSB) */
#define START_ADDR_L  0xD   /* reg index of video mem start addr (LSB) */
#define CURSOR_H      0xE   /* reg index of cursor position (MSB) */
#define CURSOR_L      0xF   /* reg index of cursor position (LSB) */

/* CMOS */
#define CLK_ELE                                         \
    0x70 /* CMOS RAM address register port (write only) \
          * Bit 7 = 1  NMI disable                      \
          *     0  NMI enable                           \
          * Bits 6-0 = RAM address                      \
          */

#define CLK_IO 0x71 /* CMOS RAM data register port (read/write) */

/* Hardware interrupts */
#define CLOCK_IRQ      0
#define KEYBOARD_IRQ   1
#define CASCADE_IRQ    2 /* cascade enable for 2nd AT controller */
#define ETHER_IRQ      3 /* default ethernet interrupt vector */
#define SECONDARY_IRQ  3 /* RS232 interrupt vector for port 2 */
#define RS232_IRQ      4 /* RS232 interrupt vector for port 1 */
#define XT_WINI_IRQ    5 /* xt winchester */
#define FLOPPY_IRQ     6 /* floppy disk */
#define PRINTER_IRQ    7
#define CMOS_CLOCK_IRQ 8
#define PS_2_IRQ       12
#define AT_WINI_IRQ    14 /* at winchester */
#define AT_WINI_1_IRQ  15

#endif
