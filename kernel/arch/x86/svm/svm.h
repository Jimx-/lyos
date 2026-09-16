/* AMD SVM backend for the Lyos hypervisor interface. */
#ifndef _ASM_X86_SVM_H_
#define _ASM_X86_SVM_H_

#include <lyos/types.h>
#include <stddef.h>
#include <asm/page.h>
#include <kernel/hypervisor.h>
#include <asm/vmx.h>

#define MSR_VM_CR       0xc0010114
#define MSR_VM_HSAVE_PA 0xc0010117
#define EFER_SVME       (1ULL << 12)
#define VM_CR_SVMDIS    (1ULL << 4)

#define SVM_CPUID_FEATURE (1U << 2)
#define SVM_FEATURE_NPT   (1U << 0)
#define SVM_FEATURE_NRIPS (1U << 3)

#define SVM_MAX_VMS        8
#define SVM_MAX_VCPUS      8
#define SVM_MAX_MEMSLOTS   8
#define SVM_NPT_POOL_PAGES 544

/* VMCB intercept bits, starting at control-area offset 0x00c. */
#define SVM_INTERCEPT_INTR       (1ULL << 0)
#define SVM_INTERCEPT_NMI        (1ULL << 1)
#define SVM_INTERCEPT_IDTR_READ  (1ULL << 6)
#define SVM_INTERCEPT_GDTR_READ  (1ULL << 7)
#define SVM_INTERCEPT_LDTR_READ  (1ULL << 8)
#define SVM_INTERCEPT_TR_READ    (1ULL << 9)
#define SVM_INTERCEPT_IDTR_WRITE (1ULL << 10)
#define SVM_INTERCEPT_GDTR_WRITE (1ULL << 11)
#define SVM_INTERCEPT_LDTR_WRITE (1ULL << 12)
#define SVM_INTERCEPT_TR_WRITE   (1ULL << 13)
#define SVM_INTERCEPT_CPUID      (1ULL << 18)
#define SVM_INTERCEPT_INVD       (1ULL << 22)
#define SVM_INTERCEPT_PAUSE      (1ULL << 23)
#define SVM_INTERCEPT_HLT        (1ULL << 24)
#define SVM_INTERCEPT_INVLPG     (1ULL << 25)
#define SVM_INTERCEPT_INVLPGA    (1ULL << 26)
#define SVM_INTERCEPT_IOIO       (1ULL << 27)
#define SVM_INTERCEPT_MSR        (1ULL << 28)
#define SVM_INTERCEPT_SHUTDOWN   (1ULL << 31)
#define SVM_INTERCEPT_VMRUN      (1ULL << 32)
#define SVM_INTERCEPT_VMMCALL    (1ULL << 33)
#define SVM_INTERCEPT_VMLOAD     (1ULL << 34)
#define SVM_INTERCEPT_VMSAVE     (1ULL << 35)
#define SVM_INTERCEPT_STGI       (1ULL << 36)
#define SVM_INTERCEPT_CLGI       (1ULL << 37)
#define SVM_INTERCEPT_SKINIT     (1ULL << 38)
#define SVM_INTERCEPT_RDTSCP     (1ULL << 39)
#define SVM_INTERCEPT_WBINVD     (1ULL << 41)
#define SVM_INTERCEPT_MONITOR    (1ULL << 42)
#define SVM_INTERCEPT_MWAIT      (1ULL << 43)
#define SVM_INTERCEPT_XSETBV     (1ULL << 45)

#define SVM_NESTED_CTL_NP_ENABLE   (1ULL << 0)
#define SVM_INT_CTL_V_INTR_MASKING (1ULL << 24)

/* SVM architectural exit codes. */
#define SVM_EXIT_READ_CR0  0x000
#define SVM_EXIT_WRITE_CR0 0x010
#define SVM_EXIT_READ_DR0  0x020
#define SVM_EXIT_WRITE_DR0 0x030
#define SVM_EXIT_EXCP_BASE 0x040
#define SVM_EXIT_INTR      0x060
#define SVM_EXIT_NMI       0x061
#define SVM_EXIT_SHUTDOWN  0x07f
#define SVM_EXIT_CPUID     0x072
#define SVM_EXIT_PAUSE     0x077
#define SVM_EXIT_HLT       0x078
#define SVM_EXIT_INVLPG    0x079
#define SVM_EXIT_IOIO      0x07b
#define SVM_EXIT_MSR       0x07c
#define SVM_EXIT_VMRUN     0x080
#define SVM_EXIT_RDTSCP    0x087
#define SVM_EXIT_WBINVD    0x089
#define SVM_EXIT_MONITOR   0x08a
#define SVM_EXIT_MWAIT     0x08b
#define SVM_EXIT_XSETBV    0x08d
#define SVM_EXIT_NPF       0x400

struct svm_segment {
    u16 selector;
    u16 attrib;
    u32 limit;
    u64 base;
} __attribute__((packed));

struct svm_vmcb_control {
    u16 intercept_cr_read;
    u16 intercept_cr_write;
    u16 intercept_dr_read;
    u16 intercept_dr_write;
    u32 intercept_exceptions;
    u64 intercept;
    u8 reserved_014[0x3c - 0x14];
    u16 pause_filter_threshold;
    u16 pause_filter_count;
    u64 iopm_base_pa;
    u64 msrpm_base_pa;
    u64 tsc_offset;
    u32 asid;
    u8 tlb_ctl;
    u8 reserved_05d[3];
    u32 int_ctl;
    u32 int_vector;
    u32 int_state;
    u32 reserved_06c;
    u64 exit_code;
    u64 exit_info1;
    u64 exit_info2;
    u64 exit_int_info;
    u64 np_enable;
    u64 avic_apic_bar;
    u64 ghcb;
    u64 event_inj;
    u64 n_cr3;
    u64 lbr_ctl;
    u32 clean;
    u32 reserved_0c4;
    u64 nrip;
    u8 insn_len;
    u8 insn_bytes[15];
    u8 reserved_0e0[0x400 - 0xe0];
} __attribute__((packed));

struct svm_vmcb_save {
    struct svm_segment es, cs, ss, ds, fs, gs;
    struct svm_segment gdtr, ldtr, idtr, tr;
    u8 reserved_4a0[0xcb - 0xa0];
    u8 cpl;
    u32 reserved_4cc;
    u64 efer;
    u8 reserved_4d8[0x548 - 0x4d8];
    u64 cr4, cr3, cr0;
    u64 dr7, dr6;
    u64 rflags, rip;
    u8 reserved_580[0x5d8 - 0x580];
    u64 rsp;
    u8 reserved_5e0[0x5f8 - 0x5e0];
    u64 rax;
    u64 star, lstar, cstar, sfmask, kernel_gs_base;
    u64 sysenter_cs, sysenter_esp, sysenter_eip;
    u64 cr2;
    u8 reserved_648[0x668 - 0x648];
    u64 g_pat, dbgctl, br_from, br_to, last_excp_from, last_excp_to;
    u8 reserved_698[0x800 - 0x698];
} __attribute__((packed));

struct svm_vmcb {
    struct svm_vmcb_control control;
    struct svm_vmcb_save save;
    u8 reserved[ARCH_PG_SIZE - 0x800];
} __attribute__((packed, aligned(ARCH_PG_SIZE)));

struct svm_memslot {
    int in_use;
    u64 gpa, len;
    struct hv_backing* backing;
};

struct svm_vcpu;
struct svm_vm {
    int in_use;
    u32 gen;
    endpoint_t owner;
    struct svm_memslot slots[SVM_MAX_MEMSLOTS];
    struct svm_vcpu* vcpu;
    u64 ncr3;
    int ever_ran;
};

struct svm_vcpu {
    u64 gprs[VMX_NR_GPRS];
    u64 vmcb_phys;
    u64 exit_rsp;
    struct vmx_guest_fpu fpu __attribute__((aligned(16)));
    struct svm_vm* vm;
    endpoint_t owner;
    u32 gen;
    int in_use;
    int state_valid;
    int running;
};

#define SVM_OFF_GPRS      0
#define SVM_OFF_VMCB_PHYS 0x080
#define SVM_OFF_EXIT_RSP  0x088

typedef int
    _svm_vmcb_control_size[sizeof(struct svm_vmcb_control) == 0x400 ? 1 : -1];
typedef int _svm_vmcb_save_size[sizeof(struct svm_vmcb_save) == 0x400 ? 1 : -1];
typedef int _svm_vmcb_size[sizeof(struct svm_vmcb) == ARCH_PG_SIZE ? 1 : -1];
typedef int _svm_vmcb_exit_code
    [offsetof(struct svm_vmcb, control.exit_code) == 0x70 ? 1 : -1];
typedef int
    _svm_vmcb_ncr3[offsetof(struct svm_vmcb, control.n_cr3) == 0xb0 ? 1 : -1];
typedef int
    _svm_vmcb_nrip[offsetof(struct svm_vmcb, control.nrip) == 0xc8 ? 1 : -1];
typedef int
    _svm_vmcb_efer[offsetof(struct svm_vmcb, save.efer) == 0x4d0 ? 1 : -1];
typedef int
    _svm_vmcb_cr0[offsetof(struct svm_vmcb, save.cr0) == 0x558 ? 1 : -1];
typedef int
    _svm_vmcb_rip[offsetof(struct svm_vmcb, save.rip) == 0x578 ? 1 : -1];
typedef int
    _svm_vmcb_rsp[offsetof(struct svm_vmcb, save.rsp) == 0x5d8 ? 1 : -1];
typedef int
    _svm_vmcb_rax[offsetof(struct svm_vmcb, save.rax) == 0x5f8 ? 1 : -1];
typedef int
    _svm_vcpu_gprs[offsetof(struct svm_vcpu, gprs) == SVM_OFF_GPRS ? 1 : -1];
typedef int _svm_vcpu_vmcb
    [offsetof(struct svm_vcpu, vmcb_phys) == SVM_OFF_VMCB_PHYS ? 1 : -1];
typedef int
    _svm_vcpu_rsp[offsetof(struct svm_vcpu, exit_rsp) == SVM_OFF_EXIT_RSP ? 1
                                                                          : -1];

extern struct svm_vcpu* svm_current_vcpu;
int svm_enter_guest(struct svm_vcpu* vcpu);
void svm_early_init(void);

int svm_npt_init_vm(struct svm_vm* vm);
void svm_npt_destroy_vm(struct svm_vm* vm);
int svm_npt_map_page(struct svm_vm* vm, u64 gpa, u64 pa, u32 flags);
void svm_npt_invalidate(struct svm_vm* vm);
u32 svm_npt_pages_in_use(void);

#endif
