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

#ifndef _ASM_VMX_H_
#define _ASM_VMX_H_

#include <lyos/types.h>
#include <stddef.h>
#include <asm/page.h>
#include <asm/vmx.h>
#include <kernel/hypervisor.h>

#define CR4_VMXE (1UL << 13)

/* VMX MSRs */
#define MSR_IA32_FEATURE_CONTROL         0x3a
#define MSR_IA32_VMX_BASIC               0x480
#define MSR_IA32_VMX_PINBASED_CTLS       0x481
#define MSR_IA32_VMX_PROCBASED_CTLS      0x482
#define MSR_IA32_VMX_EXIT_CTLS           0x483
#define MSR_IA32_VMX_ENTRY_CTLS          0x484
#define MSR_IA32_VMX_MISC                0x485
#define MSR_IA32_VMX_CR0_FIXED0          0x486
#define MSR_IA32_VMX_CR0_FIXED1          0x487
#define MSR_IA32_VMX_CR4_FIXED0          0x488
#define MSR_IA32_VMX_CR4_FIXED1          0x489
#define MSR_IA32_VMX_PROCBASED_CTLS2     0x48b
#define MSR_IA32_VMX_EPT_VPID_CAP        0x48c
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS  0x48d
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS 0x48e
#define MSR_IA32_VMX_TRUE_EXIT_CTLS      0x48f
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS     0x490

#define FEATURE_CONTROL_LOCKED          (1U << 0)
#define FEATURE_CONTROL_VMX_OUTSIDE_SMX (1U << 2)

/* VMCS field encodings */

/* 16-bit guest state */
#define VMX_GUEST_ES_SELECTOR   0x00000800
#define VMX_GUEST_CS_SELECTOR   0x00000802
#define VMX_GUEST_SS_SELECTOR   0x00000804
#define VMX_GUEST_DS_SELECTOR   0x00000806
#define VMX_GUEST_FS_SELECTOR   0x00000808
#define VMX_GUEST_GS_SELECTOR   0x0000080a
#define VMX_GUEST_LDTR_SELECTOR 0x0000080c
#define VMX_GUEST_TR_SELECTOR   0x0000080e

/* 16-bit host state */
#define VMX_HOST_ES_SELECTOR 0x00000c00
#define VMX_HOST_CS_SELECTOR 0x00000c02
#define VMX_HOST_SS_SELECTOR 0x00000c04
#define VMX_HOST_DS_SELECTOR 0x00000c06
#define VMX_HOST_FS_SELECTOR 0x00000c08
#define VMX_HOST_GS_SELECTOR 0x00000c0a
#define VMX_HOST_TR_SELECTOR 0x00000c0c

/* 64-bit control fields */
#define VMX_IO_BITMAP_A 0x00002000
#define VMX_IO_BITMAP_B 0x00002002
#define VMX_MSR_BITMAP  0x00002004
#define VMX_TSC_OFFSET  0x00002010
#define VMX_EPT_POINTER 0x0000201a

/* 64-bit read-only fields */
#define VMX_GUEST_PHYSICAL_ADDR 0x00002400

/* 64-bit guest state */
#define VMX_VMCS_LINK_PTR  0x00002800
#define VMX_GUEST_DEBUGCTL 0x00002802
#define VMX_GUEST_EFER     0x00002806

/* 32-bit control fields */
#define VMX_PINBASED_EXEC_CTL   0x00004000
#define VMX_PROCBASED_EXEC_CTL  0x00004002
#define VMX_EXCEPTION_BITMAP    0x00004004
#define VMX_VM_EXIT_CTLS        0x0000400c
#define VMX_VM_ENTRY_CTLS       0x00004012
#define VMX_PROCBASED_EXEC_CTL2 0x0000401e

#define VMX_VM_EXIT_MSR_STORE_COUNT 0x0000400e
#define VMX_VM_EXIT_MSR_LOAD_COUNT  0x00004010
#define VMX_VM_ENTRY_MSR_LOAD_COUNT 0x00004014
#define VMX_VM_ENTRY_INTR_INFO      0x00004016

/* 32-bit read-only fields */
#define VMX_INSTRUCTION_ERROR    0x00004400
#define VMX_EXIT_REASON          0x00004402
#define VMX_EXIT_INTR_INFO       0x00004404
#define VMX_EXIT_INTR_ERROR_CODE 0x00004406
#define VMX_IDT_VECTORING_INFO   0x00004408
#define VMX_IDT_VECTORING_ERROR  0x0000440a
#define VMX_EXIT_INSN_LEN        0x0000440c

/* 32-bit guest state */
#define VMX_GUEST_ES_LIMIT         0x00004800
#define VMX_GUEST_CS_LIMIT         0x00004802
#define VMX_GUEST_SS_LIMIT         0x00004804
#define VMX_GUEST_DS_LIMIT         0x00004806
#define VMX_GUEST_FS_LIMIT         0x00004808
#define VMX_GUEST_GS_LIMIT         0x0000480a
#define VMX_GUEST_LDTR_LIMIT       0x0000480c
#define VMX_GUEST_TR_LIMIT         0x0000480e
#define VMX_GUEST_GDTR_LIMIT       0x00004810
#define VMX_GUEST_IDTR_LIMIT       0x00004812
#define VMX_GUEST_ES_AR            0x00004814
#define VMX_GUEST_CS_AR            0x00004816
#define VMX_GUEST_SS_AR            0x00004818
#define VMX_GUEST_DS_AR            0x0000481a
#define VMX_GUEST_FS_AR            0x0000481c
#define VMX_GUEST_GS_AR            0x0000481e
#define VMX_GUEST_LDTR_AR          0x00004820
#define VMX_GUEST_TR_AR            0x00004822
#define VMX_GUEST_INTERRUPTIBILITY 0x00004824
#define VMX_GUEST_ACTIVITY_STATE   0x00004826
#define VMX_GUEST_SYSENTER_CS      0x0000482a

/* 32-bit host state */
#define VMX_HOST_SYSENTER_CS 0x00004c00

/* natural-width control fields */
#define VMX_CR0_GUEST_HOST_MASK 0x00006000
#define VMX_CR4_GUEST_HOST_MASK 0x00006002
#define VMX_CR0_READ_SHADOW     0x00006004
#define VMX_CR4_READ_SHADOW     0x00006006

/* natural-width read-only fields */
#define VMX_EXIT_QUALIFICATION 0x00006400
#define VMX_GUEST_LINEAR_ADDR  0x0000640a

/* natural-width guest state */
#define VMX_GUEST_CR0          0x00006800
#define VMX_GUEST_CR3          0x00006802
#define VMX_GUEST_CR4          0x00006804
#define VMX_GUEST_ES_BASE      0x00006806
#define VMX_GUEST_CS_BASE      0x00006808
#define VMX_GUEST_SS_BASE      0x0000680a
#define VMX_GUEST_DS_BASE      0x0000680c
#define VMX_GUEST_FS_BASE      0x0000680e
#define VMX_GUEST_GS_BASE      0x00006810
#define VMX_GUEST_LDTR_BASE    0x00006812
#define VMX_GUEST_TR_BASE      0x00006814
#define VMX_GUEST_GDTR_BASE    0x00006816
#define VMX_GUEST_IDTR_BASE    0x00006818
#define VMX_GUEST_DR7          0x0000681a
#define VMX_GUEST_RSP          0x0000681c
#define VMX_GUEST_RIP          0x0000681e
#define VMX_GUEST_RFLAGS       0x00006820
#define VMX_GUEST_SYSENTER_ESP 0x00006824
#define VMX_GUEST_SYSENTER_EIP 0x00006826

/* natural-width host state */
#define VMX_HOST_CR0          0x00006c00
#define VMX_HOST_CR3          0x00006c02
#define VMX_HOST_CR4          0x00006c04
#define VMX_HOST_FS_BASE      0x00006c06
#define VMX_HOST_GS_BASE      0x00006c08
#define VMX_HOST_TR_BASE      0x00006c0a
#define VMX_HOST_GDTR_BASE    0x00006c0c
#define VMX_HOST_IDTR_BASE    0x00006c0e
#define VMX_HOST_SYSENTER_ESP 0x00006c10
#define VMX_HOST_SYSENTER_EIP 0x00006c12
#define VMX_HOST_RSP          0x00006c14
#define VMX_HOST_RIP          0x00006c16

/* 64-bit host state */
#define VMX_HOST_EFER 0x00002c02

/* VMX basic msr bits */
#define VMX_BASIC_SIZE(rev)      ((rev >> 32) & 0x1fff)
#define VMX_BASIC_MEMTYPE(rev)   ((rev >> 50) & 0xf)
#define VMX_BASIC_TRUE_CTLS(rev) ((rev >> 55) & 1)
#define VMX_BASIC_REVISION(rev)  (rev & 0x7fffffff)

/* pin-based controls */
#define PIN_EXT_INTR_EXITING 0x00000001
#define PIN_NMI_EXITING      0x00000008
#define PIN_VIRTUAL_NMIS     0x00000020

/* primary processor-based controls */
#define PROC_INTR_WINDOW        0x00000004
#define PROC_USE_TSC_OFFSET     0x00000008
#define PROC_HLT_EXITING        0x00000080
#define PROC_INVLPG_EXITING     0x00000200
#define PROC_MWAIT_EXITING      0x00000400
#define PROC_RDPMC_EXITING      0x00000800
#define PROC_RDTSC_EXITING      0x00001000
#define PROC_CR3_LOAD_EXITING   0x00008000
#define PROC_CR3_STORE_EXITING  0x00010000
#define PROC_CR8_LOAD_EXITING   0x00080000
#define PROC_CR8_STORE_EXITING  0x00100000
#define PROC_MOV_DR_EXITING     0x00800000
#define PROC_UNCOND_IO_EXITING  0x01000000
#define PROC_IO_BITMAPS         0x02000000
#define PROC_MSR_BITMAPS        0x10000000
#define PROC_MONITOR_EXITING    0x20000000
#define PROC_PAUSE_EXITING      0x40000000
#define PROC_ACTIVATE_SECONDARY 0x80000000

/* secondary processor-based controls */
#define PROC2_VIRTUALIZE_APIC    0x00000001
#define PROC2_ENABLE_EPT         0x00000002
#define PROC2_DESC_TABLE_EXITING 0x00000004
#define PROC2_RDTSCP_EXITING     0x00000008
#define PROC2_VPID               0x00000020
#define PROC2_WBINVD_EXITING     0x00000040
#define PROC2_UNRESTRICTED_GUEST 0x00000080
#define PROC2_EPT_VIOLATION_VE   0x00004000

/* vm-exit controls */
#define EXIT_HOST_ADDR_SPACE_SIZE 0x00000200
#define EXIT_ACK_INTR_ON_EXIT     0x00008000
#define EXIT_SAVE_IA32_EFER       0x00100000
#define EXIT_LOAD_IA32_EFER       0x00200000

/* vm-entry controls */
#define ENTRY_LOAD_IA32_EFER 0x00008000

/* EPT/VPID capabilities (MSR_IA32_VMX_EPT_VPID_CAP) */
#define EPT_CAP_EXEC_ONLY      (1ULL << 0)
#define EPT_CAP_WALK_4         (1ULL << 6)
#define EPT_CAP_WB             (1ULL << 14)
#define EPT_CAP_2M_PAGES       (1ULL << 16)
#define EPT_CAP_INVEPT         (1ULL << 20)
#define EPT_CAP_INVEPT_SINGLE  (1ULL << 25)
#define EPT_CAP_INVEPT_ALL     (1ULL << 26)
#define EPT_CAP_INVVPID_SINGLE (1ULL << 40)

/* exit interruption info */
#define INTR_INFO_VALID       0x80000000
#define INTR_INFO_TYPE_SHIFT  8
#define INTR_INFO_TYPE_MASK   0x7
#define INTR_INFO_VECTOR_MASK 0xff
#define INTR_TYPE_EXT_INTR    0
#define INTR_TYPE_NMI         2
#define INTR_TYPE_HW_EXC      3

/* segment unusable flag in ar bytes */
#define VMX_SEG_UNUSABLE (1U << 16)

/* limits */
#define VMX_MAX_VMS        8
#define VMX_MAX_VCPUS      8
#define VMX_MAX_MEMSLOTS   8
#define VMX_EPT_POOL_PAGES 544 /* 4-level tables for 1 GiB / 4K pages */

struct vmx_mems {
    int in_use;
    u64 gpa, len;
    struct hv_backing* backing;
};

struct vmx_vcpu;

struct vmx_vm {
    int in_use;
    u32 gen;
    endpoint_t owner;
    struct vmx_mems slots[VMX_MAX_MEMSLOTS];
    struct vmx_vcpu* vcpu;
    u64 eptp; /* EPT pointer value */
    int ever_ran;
};

struct vmx_vcpu {
    u64 gprs[VMX_NR_GPRS];                                 /* 0x000 */
    u64 vmcs_phys;                                         /* 0x080 */
    u64 exit_rsp;                                          /* 0x088 */
    struct vmx_guest_fpu fpu __attribute__((aligned(16))); /* 0x090 */
    struct vmx_vm* vm;                                     /* 0x290 */
    endpoint_t owner;
    u32 gen;
    int in_use;
    int launched;    /* VMLAUNCH done, use VMRESUME */
    int state_valid; /* a valid guest state has been installed */
    int running;
    int entry_error; /* VMX instruction error of last failed launch */
};

/* offsets used by vmenter.S */
#define VMX_OFF_GPRS      0
#define VMX_OFF_VMCS_PHYS 0x080
#define VMX_OFF_EXIT_RSP  0x088
#define VMX_OFF_LAUNCHED  0x2a4

typedef int _vmx_vcpu_layout_check1
    [offsetof(struct vmx_vcpu, gprs) == VMX_OFF_GPRS ? 1 : -1];
typedef int _vmx_vcpu_layout_check2
    [offsetof(struct vmx_vcpu, vmcs_phys) == VMX_OFF_VMCS_PHYS ? 1 : -1];
typedef int _vmx_vcpu_layout_check3
    [offsetof(struct vmx_vcpu, exit_rsp) == VMX_OFF_EXIT_RSP ? 1 : -1];
typedef int _vmx_vcpu_layout_check4
    [offsetof(struct vmx_vcpu, launched) == VMX_OFF_LAUNCHED ? 1 : -1];
typedef int _vmx_vcpu_layout_check5
    [__builtin_offsetof(struct vmx_vcpu, fpu) % 16 == 0 ? 1 : -1];

/* entry/exit assembly helpers (vmenter.S) */
int vmx_enter_guest(struct vmx_vcpu* vcpu);

/* VMX backend operations. */
struct hv_caps;
struct vmx_guest_state;
struct hv_memslot;
struct vmx_exit;
struct proc;

int vmx_query_caps(struct hv_caps* caps);
int vmx_vm_create(endpoint_t owner, u32* handle);
int vmx_vm_destroy(endpoint_t owner, u32 handle);
int vmx_vm_set_memslot(endpoint_t owner, u32 handle,
                       const struct hv_memslot* slot);
int vmx_vcpu_create(endpoint_t owner, u32 vm_handle, u32* vcpu_handle);
int vmx_vcpu_destroy(endpoint_t owner, u32 vcpu_handle);
int vmx_vcpu_set_state(endpoint_t owner, u32 vcpu_handle,
                       const struct vmx_guest_state* state);
int vmx_vcpu_get_state(endpoint_t owner, u32 vcpu_handle,
                       struct vmx_guest_state* state);
int vmx_vcpu_run(endpoint_t owner, u32 vcpu_handle, struct vmx_exit* exit_out);
void vmx_proc_cleanup(struct proc* p);
void vmx_early_init(void);

#endif /* _ASM_VMX_H_ */
