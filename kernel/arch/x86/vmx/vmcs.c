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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <string.h>
#include "lyos/const.h"
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/protect.h>
#include <asm/page.h>
#include <asm/smp.h>
#include "vmx.h"
#include "vmx_priv.h"

/* raw VMCS access helpers */

static inline int vmwrite64(u32 field, u64 value)
{
    u8 fail;
    __asm__ __volatile__("vmwriteq %[val], %[field]\n\t"
                         "setna %[fail]"
                         : [fail] "=q"(fail)
                         : [field] "r"((u64)field), [val] "r"(value)
                         : "cc", "memory");
    return fail ? EINVAL : 0;
}

static inline u64 vmread64(u32 field)
{
    u64 value;
    __asm__ __volatile__("vmreadq %[field], %[val]\n\t"
                         : [val] "=r"(value)
                         : [field] "r"((u64)field)
                         : "cc");
    return value;
}

static inline int vmptrld(u64 vmcs_phys)
{
    u8 fail;
    __asm__ __volatile__("vmptrld %[pa]\n\t"
                         "setna %[fail]"
                         : [fail] "=q"(fail)
                         : [pa] "m"(vmcs_phys)
                         : "cc", "memory");
    return fail ? EINVAL : 0;
}

static inline int vmclear(u64 vmcs_phys)
{
    u8 fail;
    __asm__ __volatile__("vmclear %[pa]\n\t"
                         "setna %[fail]"
                         : [fail] "=q"(fail)
                         : [pa] "m"(vmcs_phys)
                         : "cc", "memory");
    return fail ? EINVAL : 0;
}

int vmx_vmcs_load(u64 vmcs_phys) { return vmptrld(vmcs_phys); }
int vmx_vmcs_clear(u64 vmcs_phys) { return vmclear(vmcs_phys); }
int vmx_vmwrite(u32 field, u64 value) { return vmwrite64(field, value); }
u64 vmx_vmread(u32 field) { return vmread64(field); }

/*
 * Adjust a control value against the allowed-0/allowed-1 bits of a VMX
 * control MSR: the low half specifies bits that must be 1, and the high
 * half specifies bits that may be 1.
 */
u32 vmx_adjust_control(u32 desired, u64 msr_value)
{
    u32 required1 = (u32)msr_value;
    u32 allowed1 = (u32)(msr_value >> 32);

    return (desired | required1) & allowed1;
}

/*****************************************************************************
 * VMCS setup
 *****************************************************************************/

static int write_segment(int sel_field, int limit_field, int ar_field,
                         int base_field, const struct vmx_segment* seg,
                         int force_unusable)
{
    int retval;
    u32 ar = seg->ar;

    if (force_unusable) ar |= VMX_SEG_UNUSABLE;

    if ((retval = vmwrite64(sel_field, seg->selector))) return retval;
    if ((retval = vmwrite64(limit_field, seg->limit))) return retval;
    if ((retval = vmwrite64(ar_field, ar))) return retval;
    if ((retval = vmwrite64(base_field, seg->base))) return retval;

    return 0;
}

/* synthesized, valid 32-bit busy TSS so VM entry checks pass */
static void tr_segment(struct vmx_segment* seg)
{
    memset(seg, 0, sizeof(*seg));
    seg->selector = 0x18;
    seg->limit = 0xffff;
    seg->ar = 0xb | VMX_SEG_P; /* system segment: 32-bit busy TSS, DPL 0 */
    seg->base = 0;
}

int vmx_vmcs_setup(struct vmx_vcpu* vcpu)
{
    struct vmx_vm* vm = vcpu->vm;
    struct vmx_segment seg;
    int retval;

    memset(__va(vcpu->vmcs_phys), 0, ARCH_PG_SIZE);
    *(u32*)__va(vcpu->vmcs_phys) = vmx_state.vmcs_revision;

    if ((retval = vmptrld(vcpu->vmcs_phys))) return retval;

    /* controls */
    u32 pin, proc, proc2, exit_ctl, entry_ctl;

    pin = vmx_adjust_control(PIN_EXT_INTR_EXITING | PIN_NMI_EXITING,
                             vmx_state.msr_pinbased);
    if (!(pin & PIN_EXT_INTR_EXITING)) return EIO;

    proc = vmx_adjust_control(
        PROC_HLT_EXITING | PROC_INVLPG_EXITING | PROC_MWAIT_EXITING |
            PROC_CR3_LOAD_EXITING | PROC_CR3_STORE_EXITING |
            PROC_CR8_LOAD_EXITING | PROC_CR8_STORE_EXITING |
            PROC_MOV_DR_EXITING | PROC_UNCOND_IO_EXITING |
            PROC_MONITOR_EXITING | PROC_PAUSE_EXITING | PROC_ACTIVATE_SECONDARY,
        vmx_state.msr_procbased);
    if (!(proc & PROC_ACTIVATE_SECONDARY)) return EIO;

    proc2 = vmx_adjust_control(PROC2_ENABLE_EPT | PROC2_DESC_TABLE_EXITING |
                                   PROC2_RDTSCP_EXITING | PROC2_WBINVD_EXITING |
                                   PROC2_UNRESTRICTED_GUEST,
                               vmx_state.msr_procbased2);
    if (!(proc2 & PROC2_ENABLE_EPT) || !(proc2 & PROC2_UNRESTRICTED_GUEST))
        return EIO;

    exit_ctl =
        vmx_adjust_control(EXIT_HOST_ADDR_SPACE_SIZE | EXIT_ACK_INTR_ON_EXIT |
                               EXIT_SAVE_IA32_EFER | EXIT_LOAD_IA32_EFER,
                           vmx_state.msr_exit);
    if (!(exit_ctl & EXIT_HOST_ADDR_SPACE_SIZE) ||
        !(exit_ctl & EXIT_ACK_INTR_ON_EXIT))
        return EIO;

    entry_ctl = vmx_adjust_control(ENTRY_LOAD_IA32_EFER, vmx_state.msr_entry);
    if (!(entry_ctl & ENTRY_LOAD_IA32_EFER)) return EIO;

    if ((retval = vmwrite64(VMX_PINBASED_EXEC_CTL, pin))) return retval;
    if ((retval = vmwrite64(VMX_PROCBASED_EXEC_CTL, proc))) return retval;
    if ((retval = vmwrite64(VMX_PROCBASED_EXEC_CTL2, proc2))) return retval;
    if ((retval = vmwrite64(VMX_VM_EXIT_CTLS, exit_ctl))) return retval;
    if ((retval = vmwrite64(VMX_VM_ENTRY_CTLS, entry_ctl))) return retval;

    /* VMCS memory is opaque: explicitly disable MSR lists and injection. */
    if ((retval = vmwrite64(VMX_VM_EXIT_MSR_STORE_COUNT, 0))) return retval;
    if ((retval = vmwrite64(VMX_VM_EXIT_MSR_LOAD_COUNT, 0))) return retval;
    if ((retval = vmwrite64(VMX_VM_ENTRY_MSR_LOAD_COUNT, 0))) return retval;
    if ((retval = vmwrite64(VMX_VM_ENTRY_INTR_INFO, 0))) return retval;

    /* no I/O or MSR bitmaps: all I/O and MSR accesses exit */
    if ((retval = vmwrite64(VMX_IO_BITMAP_A, 0))) return retval;
    if ((retval = vmwrite64(VMX_IO_BITMAP_B, 0))) return retval;
    if ((retval = vmwrite64(VMX_MSR_BITMAP, 0))) return retval;
    if ((retval = vmwrite64(VMX_TSC_OFFSET, 0))) return retval;

    /* every CR0/CR4 access exits; reads are served from the shadows */
    if ((retval = vmwrite64(VMX_CR0_GUEST_HOST_MASK, ~0ULL))) return retval;
    if ((retval = vmwrite64(VMX_CR4_GUEST_HOST_MASK, ~0ULL))) return retval;

    /* all guest exceptions exit (no guest IDT in this ABI version) */
    if ((retval = vmwrite64(VMX_EXCEPTION_BITMAP, ~0U))) return retval;

    if ((retval = vmwrite64(VMX_VMCS_LINK_PTR, ~0ULL))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_DEBUGCTL, 0))) return retval;

    /* EPT */
    if ((retval = vmwrite64(vmx_state.eptp_encoding, vm->eptp))) return retval;

    /* constant host state */
    if ((retval = vmwrite64(VMX_HOST_CS_SELECTOR, SELECTOR_KERNEL_CS)))
        return retval;
    if ((retval = vmwrite64(VMX_HOST_SS_SELECTOR, SELECTOR_KERNEL_DS)))
        return retval;
    if ((retval = vmwrite64(VMX_HOST_DS_SELECTOR, SELECTOR_KERNEL_DS)))
        return retval;
    if ((retval = vmwrite64(VMX_HOST_ES_SELECTOR, SELECTOR_KERNEL_DS)))
        return retval;
    if ((retval = vmwrite64(VMX_HOST_FS_SELECTOR, 0))) return retval;
    if ((retval = vmwrite64(VMX_HOST_GS_SELECTOR, 0))) return retval;
    if ((retval = vmwrite64(VMX_HOST_TR_SELECTOR, SELECTOR_TSS))) return retval;
    if ((retval = vmwrite64(VMX_HOST_SYSENTER_CS, 0))) return retval;
    if ((retval = vmwrite64(VMX_HOST_SYSENTER_ESP, 0))) return retval;
    if ((retval = vmwrite64(VMX_HOST_SYSENTER_EIP, 0))) return retval;
    if ((retval = vmwrite64(VMX_HOST_RIP, (u64)vmx_exit_entry))) return retval;

    /* synthesized guest TR and unusable LDTR */
    tr_segment(&seg);
    if ((retval = write_segment(VMX_GUEST_TR_SELECTOR, VMX_GUEST_TR_LIMIT,
                                VMX_GUEST_TR_AR, VMX_GUEST_TR_BASE, &seg, 0)))
        return retval;

    memset(&seg, 0, sizeof(seg));
    if ((retval =
             write_segment(VMX_GUEST_LDTR_SELECTOR, VMX_GUEST_LDTR_LIMIT,
                           VMX_GUEST_LDTR_AR, VMX_GUEST_LDTR_BASE, &seg, 1)))
        return retval;

    if ((retval = vmwrite64(VMX_GUEST_SYSENTER_CS, 0))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_SYSENTER_ESP, 0))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_SYSENTER_EIP, 0))) return retval;

    /* leave the VMCS clear; vmptrld is done on demand */
    vmx_state.current_vmcs = 0;
    return vmclear(vcpu->vmcs_phys);
}

/*****************************************************************************
 * Guest state validation and installation
 *****************************************************************************/

static int seg_usable(const struct vmx_segment* seg)
{
    return !(seg->ar & VMX_SEG_UNUSABLE);
}

static int validate_segment(const struct vmx_segment* seg, int is_code,
                            int is_ss)
{
    u32 ar, type;

    if (!seg_usable(seg)) {
        /* only unusable segments we accept are LDTR (handled separately) */
        return EINVAL;
    }

    if (seg->selector & 0x4) return EINVAL; /* no LDT selectors */
    if ((seg->selector & 3) != 0) return EINVAL;

    ar = seg->ar;
    if (ar & ~VMX_SEG_AR_MASK) return EINVAL;
    if (!(ar & VMX_SEG_P)) return EINVAL;
    if (!(ar & VMX_SEG_S)) return EINVAL;

    type = ar & VMX_SEG_TYPE_MASK;
    if (is_code) {
        if (type != 0xb /* non-conforming code, accessed */) return EINVAL;
        if (ar & VMX_SEG_L) return EINVAL; /* no 64-bit guest segments */
    } else {
        if ((type & 0x7) != 0x3 /* data, writable, accessed */ &&
            type != 0x1 /* readable, accessed */)
            return EINVAL;
    }
    if (is_ss && type != 0x3) return EINVAL;

    if (ar & VMX_SEG_G) {
        if ((seg->limit & 0xfff) != 0xfff) return EINVAL;
    }

    return 0;
}

int vmx_validate_state(const struct vmx_guest_state* st)
{
    if (st->version != VMX_STATE_VERSION) return EINVAL;
    if (st->size != sizeof(*st)) return EINVAL;
    if (st->flags || st->reserved0 || st->reserved1[0] || st->reserved1[1])
        return EINVAL;

    /* architectural constraints of this ABI version */
    if (st->efer != 0) return EINVAL;
    if (st->dr7 != 0x400) return EINVAL;
    if (st->activity_state != VMX_ACTIVITY_ACTIVE) return EINVAL;
    if (st->interruptibility != 0) return EINVAL;

    if (!(st->rflags & (1UL << 1))) return EINVAL; /* reserved bit must be 1 */
    if (st->rflags >> 32) return EINVAL;
    if (st->rflags & (1UL << 17)) return EINVAL; /* no VM86 */

    /*
     * Guest baseline: 32-bit protected mode with paging disabled and
     * unrestricted guest enabled, EPT on. CR0/CR4 fixed bits: PE and PG are
     * relaxed by the unrestricted guest.
     */
    {
        u64 cr0_must = vmx_state.msr_cr0_fixed0 & ~(1ULL | (1ULL << 31));
        /* VMXE is supplied in the hardware state and hidden by the mask. */
        u64 cr4_must = vmx_state.msr_cr4_fixed0 & ~(1ULL << 13);

        if ((st->cr0 & cr0_must) != cr0_must) return EINVAL;
        if (st->cr0 & ~vmx_state.msr_cr0_fixed1) return EINVAL;
        if ((st->cr4 & cr4_must) != cr4_must) return EINVAL;
        if (st->cr4 & ~vmx_state.msr_cr4_fixed1) return EINVAL;
    }

    if (!(st->cr0 & 0x1)) return EINVAL;       /* CR0.PE */
    if (st->cr0 & 0x80000000UL) return EINVAL; /* CR0.PG must be 0 */
    if (st->cr0 & (1UL << 2)) return EINVAL;   /* CR0.EM off: FPU usable */
    if (st->cr0 & (1UL << 3)) return EINVAL;   /* CR0.TS off */
    if (st->cr0 & (1UL << 29)) return EINVAL;  /* CR0.NW */

    if (st->cr4 & (1UL << 13)) return EINVAL;   /* no VMXE for guests */
    if (st->cr4 & (1UL << 17)) return EINVAL;   /* no PCIDE */
    if (st->cr4 & (1UL << 18)) return EINVAL;   /* no OSXSAVE */
    if (st->cr4 & (1UL << 5)) return EINVAL;    /* no PAE */
    if (!(st->cr4 & (1UL << 9))) return EINVAL; /* OSFXSR required */
    if ((st->cr4 >> 32)) return EINVAL;

    if (st->fpu.mxcsr & ~vmx_state.mxcsr_mask) return EINVAL;

    int i;
    for (i = 0; i < VMX_NR_GPRS; i++) {
        if (st->gprs[i] >> 32) return EINVAL; /* 32-bit guest */
    }
    if (st->rip >> 32 || st->gprs[VMX_GPR_RSP] >> 32) return EINVAL;

    if (validate_segment(&st->cs, 1, 0)) return EINVAL;
    if (validate_segment(&st->ss, 0, 1)) return EINVAL;
    if (validate_segment(&st->ds, 0, 0)) return EINVAL;
    if (validate_segment(&st->es, 0, 0)) return EINVAL;
    if (validate_segment(&st->fs, 0, 0)) return EINVAL;
    if (validate_segment(&st->gs, 0, 0)) return EINVAL;

    return 0;
}

int vmx_vmcs_write_state(struct vmx_vcpu* vcpu,
                         const struct vmx_guest_state* st)
{
    static const struct {
        int sel, limit, ar, base;
        int is_code, is_ss;
    } segs[6] = {
        {VMX_GUEST_CS_SELECTOR, VMX_GUEST_CS_LIMIT, VMX_GUEST_CS_AR,
         VMX_GUEST_CS_BASE, 1, 0},
        {VMX_GUEST_SS_SELECTOR, VMX_GUEST_SS_LIMIT, VMX_GUEST_SS_AR,
         VMX_GUEST_SS_BASE, 0, 1},
        {VMX_GUEST_DS_SELECTOR, VMX_GUEST_DS_LIMIT, VMX_GUEST_DS_AR,
         VMX_GUEST_DS_BASE, 0, 0},
        {VMX_GUEST_ES_SELECTOR, VMX_GUEST_ES_LIMIT, VMX_GUEST_ES_AR,
         VMX_GUEST_ES_BASE, 0, 0},
        {VMX_GUEST_FS_SELECTOR, VMX_GUEST_FS_LIMIT, VMX_GUEST_FS_AR,
         VMX_GUEST_FS_BASE, 0, 0},
        {VMX_GUEST_GS_SELECTOR, VMX_GUEST_GS_LIMIT, VMX_GUEST_GS_AR,
         VMX_GUEST_GS_BASE, 0, 0},
    };
    const struct vmx_segment* sgs[6];
    int i, retval;

    sgs[0] = &st->cs;
    sgs[1] = &st->ss;
    sgs[2] = &st->ds;
    sgs[3] = &st->es;
    sgs[4] = &st->fs;
    sgs[5] = &st->gs;

    for (i = 0; i < 6; i++) {
        if ((retval = write_segment(segs[i].sel, segs[i].limit, segs[i].ar,
                                    segs[i].base, sgs[i], 0)))
            return retval;
    }

    if ((retval = vmwrite64(VMX_GUEST_GDTR_BASE, st->gdtr.base))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_GDTR_LIMIT, st->gdtr.limit)))
        return retval;
    if ((retval = vmwrite64(VMX_GUEST_IDTR_BASE, st->idtr.base))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_IDTR_LIMIT, st->idtr.limit)))
        return retval;

    if ((retval = vmwrite64(VMX_GUEST_CR0, st->cr0))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_CR3, st->cr3))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_CR4, st->cr4 | vmx_state.msr_cr4_fixed0)))
        return retval;
    if ((retval = vmwrite64(VMX_CR0_READ_SHADOW, st->cr0))) return retval;
    if ((retval = vmwrite64(VMX_CR4_READ_SHADOW, st->cr4))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_EFER, st->efer))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_DR7, st->dr7))) return retval;

    if ((retval = vmwrite64(VMX_GUEST_RFLAGS, st->rflags))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_RIP, st->rip))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_RSP, st->gprs[VMX_GPR_RSP])))
        return retval;

    if ((retval = vmwrite64(VMX_GUEST_INTERRUPTIBILITY, 0))) return retval;
    if ((retval = vmwrite64(VMX_GUEST_ACTIVITY_STATE, 0))) return retval;

    return 0;
}

void vmx_vmcs_read_state(struct vmx_vcpu* vcpu, struct vmx_guest_state* st)
{
    memset(st, 0, sizeof(*st));
    st->version = VMX_STATE_VERSION;
    st->size = sizeof(*st);

    memcpy(st->gprs, vcpu->gprs, sizeof(st->gprs));
    st->gprs[VMX_GPR_RSP] = vmread64(VMX_GUEST_RSP);
    st->fpu = vcpu->fpu;

    st->rip = vmread64(VMX_GUEST_RIP);
    st->rflags = vmread64(VMX_GUEST_RFLAGS);
    st->cr0 = vmread64(VMX_GUEST_CR0);
    st->cr3 = vmread64(VMX_GUEST_CR3);
    /* All CR4 bits are masked; expose the guest-visible shadow. */
    st->cr4 = vmread64(VMX_CR4_READ_SHADOW);
    st->efer = vmread64(VMX_GUEST_EFER);
    st->dr7 = vmread64(VMX_GUEST_DR7);
    st->activity_state = VMX_ACTIVITY_ACTIVE;
    st->interruptibility = 0;

#define READ_SEG(seg, SEL, LIMIT, AR, BASE)               \
    do {                                                  \
        (seg).selector = (u16)vmread64(SEL);              \
        (seg).limit = (u32)vmread64(LIMIT);               \
        (seg).ar = (u32)vmread64(AR) & ~VMX_SEG_UNUSABLE; \
        (seg).base = vmread64(BASE);                      \
    } while (0)

    READ_SEG(st->cs, VMX_GUEST_CS_SELECTOR, VMX_GUEST_CS_LIMIT, VMX_GUEST_CS_AR,
             VMX_GUEST_CS_BASE);
    READ_SEG(st->ss, VMX_GUEST_SS_SELECTOR, VMX_GUEST_SS_LIMIT, VMX_GUEST_SS_AR,
             VMX_GUEST_SS_BASE);
    READ_SEG(st->ds, VMX_GUEST_DS_SELECTOR, VMX_GUEST_DS_LIMIT, VMX_GUEST_DS_AR,
             VMX_GUEST_DS_BASE);
    READ_SEG(st->es, VMX_GUEST_ES_SELECTOR, VMX_GUEST_ES_LIMIT, VMX_GUEST_ES_AR,
             VMX_GUEST_ES_BASE);
    READ_SEG(st->fs, VMX_GUEST_FS_SELECTOR, VMX_GUEST_FS_LIMIT, VMX_GUEST_FS_AR,
             VMX_GUEST_FS_BASE);
    READ_SEG(st->gs, VMX_GUEST_GS_SELECTOR, VMX_GUEST_GS_LIMIT, VMX_GUEST_GS_AR,
             VMX_GUEST_GS_BASE);
#undef READ_SEG

    st->gdtr.base = vmread64(VMX_GUEST_GDTR_BASE);
    st->gdtr.limit = (u32)vmread64(VMX_GUEST_GDTR_LIMIT);
    st->idtr.base = vmread64(VMX_GUEST_IDTR_BASE);
    st->idtr.limit = (u32)vmread64(VMX_GUEST_IDTR_LIMIT);
}

/*
 * Refresh host state that may change between entries: CR0/CR3/CR4, FS and
 * GS bases, EFER, and the exit stack.
 */
int vmx_vmcs_refresh_host(struct vmx_vcpu* vcpu)
{
    struct {
        u16 limit;
        u64 base;
    } __attribute__((packed)) gdtr, idtr;
    u32 hi, lo;
    int retval;

    __asm__ __volatile__("sgdt %0" : "=m"(gdtr));
    __asm__ __volatile__("sidt %0" : "=m"(idtr));
    if ((retval = vmwrite64(VMX_HOST_GDTR_BASE, gdtr.base))) return retval;
    if ((retval = vmwrite64(VMX_HOST_IDTR_BASE, idtr.base))) return retval;

    if ((retval = vmwrite64(VMX_HOST_CR0, read_cr0()))) return retval;
    if ((retval = vmwrite64(VMX_HOST_CR3, read_cr3()))) return retval;
    if ((retval = vmwrite64(VMX_HOST_CR4, read_cr4()))) return retval;

    ia32_read_msr(AMD_MSR_FS_BASE, &hi, &lo);
    if ((retval = vmwrite64(VMX_HOST_FS_BASE, make64(hi, lo)))) return retval;
    ia32_read_msr(AMD_MSR_GS_BASE, &hi, &lo);
    if ((retval = vmwrite64(VMX_HOST_GS_BASE, make64(hi, lo)))) return retval;
    ia32_read_msr(AMD_MSR_EFER, &hi, &lo);
    if ((retval = vmwrite64(VMX_HOST_EFER, make64(hi, lo)))) return retval;

    if ((retval = vmwrite64(VMX_HOST_TR_BASE, (u64)&tss[cpuid]))) return retval;
    if ((retval = vmwrite64(VMX_HOST_RSP, vmx_state.exit_rsp))) return retval;

    return 0;
}
