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

#ifndef _UAPI_ASM_VMX_H_
#define _UAPI_ASM_VMX_H_

#include <lyos/types.h>

/* General purpose register indices (struct vmx_guest_state.gprs) */
#define VMX_GPR_RAX 0
#define VMX_GPR_RCX 1
#define VMX_GPR_RDX 2
#define VMX_GPR_RBX 3
#define VMX_GPR_RSP 4
#define VMX_GPR_RBP 5
#define VMX_GPR_RSI 6
#define VMX_GPR_RDI 7
#define VMX_GPR_R8  8
#define VMX_GPR_R9  9
#define VMX_GPR_R10 10
#define VMX_GPR_R11 11
#define VMX_GPR_R12 12
#define VMX_GPR_R13 13
#define VMX_GPR_R14 14
#define VMX_GPR_R15 15
#define VMX_NR_GPRS 16

#define VMX_STATE_VERSION 1

/*
 * Architectural segment attributes ("ar"), one dword per segment:
 *   bits  3:0  type
 *   bit   4    S (1 = code/data, 0 = system)
 *   bits  6:5  DPL
 *   bit   7    P (present)
 *   bit   12   AVL
 *   bit   13   L (64-bit code segment)
 *   bit   14   D/B
 *   bit   15   G (limit granularity)
 * all other bits must be zero.
 */
#define VMX_SEG_TYPE_MASK 0xf
#define VMX_SEG_S         (1U << 4)
#define VMX_SEG_DPL_SHIFT 5
#define VMX_SEG_DPL_MASK  (3U << VMX_SEG_DPL_SHIFT)
#define VMX_SEG_P         (1U << 7)
#define VMX_SEG_AVL       (1U << 12)
#define VMX_SEG_L         (1U << 13)
#define VMX_SEG_DB        (1U << 14)
#define VMX_SEG_G         (1U << 15)
#define VMX_SEG_AR_MASK   0x0000f0ff /* allowed bits; rest MBZ */

struct vmx_segment {
    __u16 selector;
    __u16 reserved1;
    __u32 limit; /* bytes, granularity applied via ar G bit */
    __u32 ar;
    __u32 reserved2;
    __u64 base;
};

struct vmx_dtr {
    __u64 base;
    __u32 limit; /* in bytes */
    __u32 reserved;
};

/*
 * Guest FPU state, x87/SSE only (AVX and XSAVE are not offered to guests).
 * Fields follow the architectural FXSAVE64 layout. Only the low byte of
 * ftw is used. MXCSR must fit the host-supported mask returned in mxcsr_mask.
 */
struct vmx_guest_fpu {
    __u16 cwd;
    __u16 swd;
    __u16 ftw;
    __u16 fop;
    __u64 fip;
    __u64 fdp;
    __u32 mxcsr;
    __u32 mxcsr_mask;
    __u8 st[8][16];
    __u8 xmm[16][16];
    __u8 reserved[96];
};

/* activity states */
#define VMX_ACTIVITY_ACTIVE    0
#define VMX_ACTIVITY_HLT       1
#define VMX_ACTIVITY_SHUTDOWN  2
#define VMX_ACTIVITY_WAIT_SIPI 3

struct vmx_guest_state {
    __u32 version;
    __u32 size; /* sizeof(struct vmx_guest_state) */
    __u32 flags;
    __u32 reserved0;
    __u64 gprs[VMX_NR_GPRS];
    __u64 rip;
    __u64 rflags;
    struct vmx_segment cs, ss, ds, es, fs, gs;
    struct vmx_dtr gdtr, idtr;
    __u64 cr0;
    __u64 cr3;
    __u64 cr4;
    __u64 efer;             /* must be zero in this ABI version */
    __u64 dr7;              /* must be 0x400 */
    __u32 activity_state;   /* VMX_ACTIVITY_ACTIVE only */
    __u32 interruptibility; /* must be zero */
    __u32 reserved1[2];
    struct vmx_guest_fpu fpu;
};

/* exit record flags */
#define VMX_EXIT_F_GPA      0x1
#define VMX_EXIT_F_GLA      0x2
#define VMX_EXIT_F_INSN_LEN 0x4

struct vmx_exit {
    __u32 reason; /* VMX_EXIT_* (architectural basic exit reason) */
    __u32 flags;
    __u32 insn_len;
    __u32 intr_info; /* raw VM-exit interruption information */
    __u32 intr_error;
    __u32 idt_vectoring;
    __u32 entry_failure; /* nonzero for VM-entry failure exits */
    __u64 qualification;
    __u64 gpa;
    __u64 gla;
    __u64 rip; /* guest rip at the time of exit */
    __u64 rflags;
    __u32 reserved[6];
};

/* architectural basic exit reasons of interest */
#define VMX_EXIT_EXCEPTION            0
#define VMX_EXIT_EXTERNAL_INTERRUPT   1
#define VMX_EXIT_TRIPLE_FAULT         2
#define VMX_EXIT_CPUID                10
#define VMX_EXIT_HLT                  12
#define VMX_EXIT_INVLPG               14
#define VMX_EXIT_CR_ACCESS            28
#define VMX_EXIT_DR_ACCESS            29
#define VMX_EXIT_IO_INSTRUCTION       30
#define VMX_EXIT_RDMSR                31
#define VMX_EXIT_WRMSR                32
#define VMX_EXIT_ENTRY_FAIL_GUEST     33
#define VMX_EXIT_ENTRY_FAIL_MSR_LOAD  34
#define VMX_EXIT_MWAIT                36
#define VMX_EXIT_MONITOR_TRAP_FLAG    37
#define VMX_EXIT_MONITOR              39
#define VMX_EXIT_PAUSE                40
#define VMX_EXIT_ENTRY_FAIL_MC        41
#define VMX_EXIT_GDTR_IDTR_ACCESS     46
#define VMX_EXIT_LDTR_TR_ACCESS       47
#define VMX_EXIT_EPT_VIOLATION        48
#define VMX_EXIT_EPT_MISCONFIGURATION 49
#define VMX_EXIT_INVEPT               50
#define VMX_EXIT_PREEMPTION_TIMER     52
#define VMX_EXIT_INVVPID              53
#define VMX_EXIT_WBINVD               54
#define VMX_EXIT_XSETBV               55

/* EPT violation qualification bits */
#define VMX_EPT_VIOL_READ      0x1
#define VMX_EPT_VIOL_WRITE     0x2
#define VMX_EPT_VIOL_FETCH     0x4
#define VMX_EPT_VIOL_GLA_VALID (1U << 7)

/* I/O instruction qualification decode (architectural) */
#define VMX_IO_PORT(qual)      (((qual) >> 16) & 0xffff)
#define VMX_IO_WIDTH(qual)     (((qual) & 7) + 1) /* 1, 2 or 4 bytes */
#define VMX_IO_IS_IN(qual)     ((qual) & (1U << 3))
#define VMX_IO_IS_STRING(qual) ((qual) & (1U << 4))

#endif /* _UAPI_ASM_VMX_H_ */
