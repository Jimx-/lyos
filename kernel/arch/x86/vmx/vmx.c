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
#include <kernel/global.h>
#include <kernel/proto.h>
#include <kernel/irq.h>
#include <asm/proto.h>
#include <asm/const.h>
#include <asm/protect.h>
#include <asm/smp.h>
#include <asm/cpu_info.h>
#include <asm/page.h>
#include <lyos/vm.h>
#include "vmx.h"
#include "vmx_priv.h"

#if CONFIG_X86_LOCAL_APIC
#include "../apic.h"
extern void apic_timer_int_handler(void);
#endif

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];

extern struct tss tss[];

struct vmx_state_info vmx_state;
struct vmx_vcpu* vmx_current_vcpu;

/* static resource pools */
static u8 vmxon_region[ARCH_PG_SIZE] __attribute__((aligned(ARCH_PG_SIZE)));
static u8 vmcs_probe_region[ARCH_PG_SIZE]
    __attribute__((aligned(ARCH_PG_SIZE)));

static u8 vmcs_regions[VMX_MAX_VCPUS][ARCH_PG_SIZE]
    __attribute__((aligned(ARCH_PG_SIZE)));

static struct vmx_vm vm_table[VMX_MAX_VMS];
static struct vmx_vcpu vcpu_table[VMX_MAX_VCPUS];

static const struct hv_backend_ops vmx_backend_ops;

static void probe_mxcsr_mask(void)
{
    /* Boot stack entry does not guarantee the C ABI's stack alignment. */
    static u8 image[512] __attribute__((aligned(16)));
    unsigned long cr0 = read_cr0();

    memset(image, 0, sizeof(image));
    disable_fpu_exception();
    __asm__ __volatile__("fxsave64 %0" : "=m"(image));
    write_cr0(cr0);
    vmx_state.mxcsr_mask = *(u32*)(image + 28);
    if (!vmx_state.mxcsr_mask) vmx_state.mxcsr_mask = 0xffbf;
}

/*****************************************************************************
 *                        capability probing and VMXON
 *****************************************************************************/

static void read_ctl_msrs(void)
{
    u32 hi, lo;

#define READ_MSR(msr, out)            \
    do {                              \
        ia32_read_msr(msr, &hi, &lo); \
        (out) = make64(hi, lo);       \
    } while (0)

    READ_MSR(MSR_IA32_VMX_BASIC, vmx_state.msr_basic);

    if (VMX_BASIC_TRUE_CTLS(vmx_state.msr_basic)) {
        READ_MSR(MSR_IA32_VMX_TRUE_PINBASED_CTLS, vmx_state.msr_pinbased);
        READ_MSR(MSR_IA32_VMX_TRUE_PROCBASED_CTLS, vmx_state.msr_procbased);
        READ_MSR(MSR_IA32_VMX_TRUE_EXIT_CTLS, vmx_state.msr_exit);
        READ_MSR(MSR_IA32_VMX_TRUE_ENTRY_CTLS, vmx_state.msr_entry);
    } else {
        READ_MSR(MSR_IA32_VMX_PINBASED_CTLS, vmx_state.msr_pinbased);
        READ_MSR(MSR_IA32_VMX_PROCBASED_CTLS, vmx_state.msr_procbased);
        READ_MSR(MSR_IA32_VMX_EXIT_CTLS, vmx_state.msr_exit);
        READ_MSR(MSR_IA32_VMX_ENTRY_CTLS, vmx_state.msr_entry);
    }
    READ_MSR(MSR_IA32_VMX_PROCBASED_CTLS2, vmx_state.msr_procbased2);
    READ_MSR(MSR_IA32_VMX_EPT_VPID_CAP, vmx_state.ept_vpid_cap);
    READ_MSR(MSR_IA32_VMX_CR0_FIXED0, vmx_state.msr_cr0_fixed0);
    READ_MSR(MSR_IA32_VMX_CR0_FIXED1, vmx_state.msr_cr0_fixed1);
    READ_MSR(MSR_IA32_VMX_CR4_FIXED0, vmx_state.msr_cr4_fixed0);
    READ_MSR(MSR_IA32_VMX_CR4_FIXED1, vmx_state.msr_cr4_fixed1);
#undef READ_MSR
}

static inline int vmxon(u64 pa)
{
    u8 fail;
    __asm__ __volatile__("vmxon %[pa]\n\t"
                         "setna %[fail]"
                         : [fail] "=q"(fail)
                         : [pa] "m"(pa)
                         : "cc", "memory");
    return fail ? EINVAL : 0;
}

/*
 * Verify the architectural EPT-pointer field at boot using write/readback
 * on a scratch VMCS.
 */
static u32 probe_eptp_encoding(void)
{
    u32 revision = vmx_state.vmcs_revision;
    u32 encoding = 0;

    memset(vmcs_probe_region, 0, ARCH_PG_SIZE);
    *(u32*)vmcs_probe_region = revision;

    if (vmx_vmcs_load(__pa(vmcs_probe_region))) return 0;

    /* methodology check with an encoding that is certain */
    if (vmx_vmwrite(VMX_TSC_OFFSET, 0x1234567890abcdefULL)) goto out;
    if (vmx_vmread(VMX_TSC_OFFSET) != 0x1234567890abcdefULL) goto out;

    if (vmx_vmwrite(VMX_EPT_POINTER, 0x000000123456701eULL)) goto out;
    if (vmx_vmread(VMX_EPT_POINTER) == 0x000000123456701eULL)
        encoding = VMX_EPT_POINTER;

out:
    vmx_vmcs_clear(__pa(vmcs_probe_region));
    vmx_state.current_vmcs = 0;
    return encoding;
}

void vmx_early_init(void)
{
    u32 hi, lo, feature_control;
    u64 basic;

    memset(&vmx_state, 0, sizeof(vmx_state));
    memset(vm_table, 0, sizeof(vm_table));
    memset(vcpu_table, 0, sizeof(vcpu_table));

    if (cpu_info[cpuid].vendor != CPU_VENDOR_INTEL) {
        printk("vmx: unavailable: CPU vendor is not Intel\n");
        return;
    }
    if (!(cpu_info[cpuid].flags[0] & (1U << 5))) {
        printk("vmx: unavailable: CPUID does not expose VMX\n");
        return;
    } /* CPUID.1.ECX.VMX */

    ia32_read_msr(MSR_IA32_FEATURE_CONTROL, &hi, &lo);
    feature_control = lo;
    if (feature_control & FEATURE_CONTROL_LOCKED) {
        if (!(feature_control & FEATURE_CONTROL_VMX_OUTSIDE_SMX)) {
            printk("vmx: unavailable: IA32_FEATURE_CONTROL disables VMX "
                   "outside SMX\n");
            return;
        }
    } else {
        ia32_write_msr(MSR_IA32_FEATURE_CONTROL, 0,
                       feature_control | FEATURE_CONTROL_LOCKED |
                           FEATURE_CONTROL_VMX_OUTSIDE_SMX);
    }

    read_ctl_msrs();
    basic = vmx_state.msr_basic;

    if (VMX_BASIC_SIZE(basic) > ARCH_PG_SIZE) {
        printk("vmx: unavailable: VMCS region exceeds one page\n");
        return;
    }
    if (VMX_BASIC_MEMTYPE(basic) != 6) {
        printk("vmx: unavailable: VMCS memory type is not write-back\n");
        return;
    } /* WB only */
    vmx_state.vmcs_revision = VMX_BASIC_REVISION(basic);

    if (!vmx_ept_supported()) {
        printk(
            "vmx: unavailable: required EPT/INVEPT capabilities are missing\n");
        return;
    }

    /* verify the control set this design depends on */
    {
        u32 pin, proc, proc2, ex, en;

        pin = vmx_adjust_control(PIN_EXT_INTR_EXITING | PIN_NMI_EXITING,
                                 vmx_state.msr_pinbased);
        if (!(pin & PIN_EXT_INTR_EXITING)) {
            printk("vmx: unavailable: external-interrupt exiting is "
                   "unavailable\n");
            return;
        }

        proc = vmx_adjust_control(
            PROC_HLT_EXITING | PROC_INVLPG_EXITING | PROC_MWAIT_EXITING |
                PROC_CR3_LOAD_EXITING | PROC_CR3_STORE_EXITING |
                PROC_CR8_LOAD_EXITING | PROC_CR8_STORE_EXITING |
                PROC_MOV_DR_EXITING | PROC_UNCOND_IO_EXITING |
                PROC_MONITOR_EXITING | PROC_PAUSE_EXITING |
                PROC_ACTIVATE_SECONDARY,
            vmx_state.msr_procbased);
        if (!(proc & PROC_ACTIVATE_SECONDARY)) {
            printk("vmx: unavailable: secondary execution controls are "
                   "unavailable\n");
            return;
        }

        proc2 = vmx_adjust_control(
            PROC2_ENABLE_EPT | PROC2_DESC_TABLE_EXITING | PROC2_RDTSCP_EXITING |
                PROC2_WBINVD_EXITING | PROC2_UNRESTRICTED_GUEST,
            vmx_state.msr_procbased2);
        if (!(proc2 & PROC2_ENABLE_EPT)) {
            printk("vmx: unavailable: EPT control is unavailable\n");
            return;
        }
        if (!(proc2 & PROC2_UNRESTRICTED_GUEST)) {
            printk("vmx: unavailable: unrestricted guest is unavailable\n");
            return;
        }

        ex = vmx_adjust_control(EXIT_HOST_ADDR_SPACE_SIZE |
                                    EXIT_ACK_INTR_ON_EXIT |
                                    EXIT_SAVE_IA32_EFER | EXIT_LOAD_IA32_EFER,
                                vmx_state.msr_exit);
        if (!(ex & EXIT_HOST_ADDR_SPACE_SIZE)) {
            printk("vmx: unavailable: 64-bit host mode is unavailable\n");
            return;
        }
        if (!(ex & EXIT_ACK_INTR_ON_EXIT)) {
            printk("vmx: unavailable: acknowledge-interrupt-on-exit is "
                   "unavailable\n");
            return;
        }

        en = vmx_adjust_control(ENTRY_LOAD_IA32_EFER, vmx_state.msr_entry);
        if (!(en & ENTRY_LOAD_IA32_EFER)) {
            printk("vmx: unavailable: loading guest EFER is unavailable\n");
            return;
        }
    }

    /* enable VMX operation and allocate the VMXON region */
    *(u32*)vmxon_region = vmx_state.vmcs_revision;
    write_cr4(read_cr4() | CR4_VMXE);
    if (vmxon(__pa(vmxon_region))) {
        printk("vmx: unavailable: VMXON failed\n");
        write_cr4(read_cr4() & ~CR4_VMXE);
        return;
    }
    vmx_state.vmxon_active = 1;

    vmx_state.eptp_encoding = probe_eptp_encoding();
    if (!vmx_state.eptp_encoding) {
        printk("vmx: unavailable: EPT-pointer VMCS probe failed\n");
        /* EPT pointer not manageable: no EPT, no VMs */
        vmx_state.caps = 0;
        return;
    }

    vmx_state.exit_rsp = (u64)vmx_exit_stack_top;
    probe_mxcsr_mask();

    vmx_state.caps = HV_CAP_STAGE2 | HV_CAP_X86_32 | HV_CAP_ACTIVE;
    if (hv_backend_register(&vmx_backend_ops))
        panic("vmx: cannot register hypervisor backend");
    printk("vmx: VT-x active, EPT and unrestricted guest available\n");
}

/*****************************************************************************
 *                        handles
 *****************************************************************************/

#define HANDLE(gen, idx) ((((u32)(gen)) << 8) | ((u32)(idx)))
#define HANDLE_IDX(h)    ((h) & 0xff)
#define HANDLE_GEN(h)    (((h) >> 8) & 0xffffff)

static struct vmx_vm* lookup_vm(u32 handle)
{
    u32 idx = HANDLE_IDX(handle);
    if (idx >= VMX_MAX_VMS) return NULL;
    struct vmx_vm* vm = &vm_table[idx];
    if (!vm->in_use || vm->gen != HANDLE_GEN(handle)) return NULL;
    return vm;
}

static struct vmx_vcpu* lookup_vcpu(u32 handle)
{
    u32 idx = HANDLE_IDX(handle);
    if (idx >= VMX_MAX_VCPUS) return NULL;
    struct vmx_vcpu* vcpu = &vcpu_table[idx];
    if (!vcpu->in_use || vcpu->gen != HANDLE_GEN(handle)) return NULL;
    return vcpu;
}

/*****************************************************************************
 *                        VM lifecycle
 *****************************************************************************/

int vmx_query_caps(struct hv_caps* caps)
{
    memset(caps, 0, sizeof(*caps));
    caps->abi_version = HV_ABI_VERSION;
    caps->caps = vmx_state.caps;
    caps->translation_pages = vmx_ept_pages_in_use();

    int i, vms = 0, vcpus = 0;
    for (i = 0; i < VMX_MAX_VMS; i++)
        if (vm_table[i].in_use) vms++;
    for (i = 0; i < VMX_MAX_VCPUS; i++)
        if (vcpu_table[i].in_use) vcpus++;
    caps->active_vms = vms;
    caps->active_vcpus = vcpus;

    return 0;
}

int vmx_vm_create(endpoint_t owner, u32* handle)
{
    struct vmx_vm* vm = NULL;
    u32 gen;
    int i;

    if (!vmx_state.vmxon_active) return EOPNOTSUPP;

    for (i = 0; i < VMX_MAX_VMS; i++) {
        if (!vm_table[i].in_use) {
            vm = &vm_table[i];
            break;
        }
    }
    if (!vm) return ENOSPC;

    gen = vm->gen + 1; /* preserved across memset so handles are not reused */
    memset(vm, 0, sizeof(*vm));
    if (vmx_ept_init_vm(vm) != 0) {
        vm->gen = gen;
        return ENOMEM;
    }

    vm->in_use = 1;
    vm->gen = gen & 0xffffff;
    if (vm->gen == 0) vm->gen = 1;
    vm->owner = owner;

    *handle = HANDLE(vm->gen, i);
    return 0;
}

static void destroy_vcpu_internal(struct vmx_vcpu* vcpu)
{
    /* the VMCS must be clear before its memory can be reused */
    vmx_vmcs_clear(vcpu->vmcs_phys);
    if (vmx_state.current_vmcs == vcpu->vmcs_phys) vmx_state.current_vmcs = 0;

    vcpu->in_use = 0;
    vcpu->gen = (vcpu->gen + 1) & 0xffffff;
    if (vcpu->gen == 0) vcpu->gen = 1;
}

int vmx_vm_destroy(endpoint_t owner, u32 handle)
{
    struct vmx_vm* vm = lookup_vm(handle);
    int i;

    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;

    if (vm->vcpu) {
        if (vm->vcpu->running) return EBUSY;
        destroy_vcpu_internal(vm->vcpu);
    }

    vmx_ept_destroy_vm(vm);

    for (i = 0; i < VMX_MAX_MEMSLOTS; i++) {
        struct vmx_mems* slot = &vm->slots[i];
        if (slot->in_use) {
            if (slot->backing) hv_backing_put(slot->backing);
            slot->in_use = 0;
        }
    }

    vm->in_use = 0;
    vm->gen = (vm->gen + 1) & 0xffffff;
    if (vm->gen == 0) vm->gen = 1;

    return 0;
}

int vmx_vm_set_memslot(endpoint_t owner, u32 handle,
                       const struct hv_memslot* slot)
{
    struct vmx_vm* vm = lookup_vm(handle);
    struct hv_backing* b;
    unsigned long i, npages, first, last;
    u64 gpa_end;

    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;
    if (vm->ever_ran) return EPERM; /* layout is fixed after the first run */

    if (slot->gpa & (ARCH_PG_SIZE - 1)) return EINVAL;
    if (slot->len == 0 || (slot->len & (ARCH_PG_SIZE - 1))) return EINVAL;
    if (slot->offset & (ARCH_PG_SIZE - 1)) return EINVAL;
    if ((slot->flags & ~HV_MEM_ALL) || !slot->flags) return EINVAL;

    gpa_end = slot->gpa + slot->len;
    if (gpa_end < slot->gpa) return EINVAL;    /* overflow */
    if (gpa_end > (1ULL << 46)) return EINVAL; /* EPT reachable range */

    b = hv_backing_lookup(slot->mm_handle);
    if (!b) return ENOENT;
    if (slot->offset > b->npages * ARCH_PG_SIZE ||
        slot->len > b->npages * ARCH_PG_SIZE - slot->offset)
        return EINVAL;

    /* no overlapping slots */
    for (i = 0; i < VMX_MAX_MEMSLOTS; i++) {
        struct vmx_mems* s = &vm->slots[i];
        if (!s->in_use) continue;
        if (slot->gpa < s->gpa + s->len && s->gpa < gpa_end) return EBUSY;
    }

    /* find a free slot */
    for (i = 0; i < VMX_MAX_MEMSLOTS; i++) {
        if (!vm->slots[i].in_use) break;
    }
    if (i == VMX_MAX_MEMSLOTS) return ENOSPC;
    unsigned slot_idx = i;

    npages = slot->len >> ARCH_PG_SHIFT;
    first = slot->offset >> ARCH_PG_SHIFT;
    last = first + npages;

    /*
     * Precheck the EPT pool so mapping cannot fail halfway; worst case the
     * region needs a new table every 512 pages plus up to three upper levels.
     */
    {
        u32 need = npages / 512 + 4;
        if (vmx_ept_pages_in_use() + need > VMX_EPT_POOL_PAGES) return ENOMEM;
    }

    for (i = first; i < last; i++) {
        u64 pa = hv_backing_page(b, i);
        if (vmx_ept_map_page(vm, slot->gpa + ((i - first) << ARCH_PG_SHIFT), pa,
                             slot->flags & 7) != 0) {
            /* should not happen after the precheck; freeze the VM layout */
            vm->ever_ran = 1;
            return ENOMEM;
        }
    }

    vm->slots[slot_idx].in_use = 1;
    vm->slots[slot_idx].gpa = slot->gpa;
    vm->slots[slot_idx].len = slot->len;
    vm->slots[slot_idx].backing = b;
    hv_backing_get(b);

    vmx_ept_invalidate(vm);
    return 0;
}

/*****************************************************************************
 *                        vCPU lifecycle
 *****************************************************************************/

int vmx_vcpu_create(endpoint_t owner, u32 vm_handle, u32* vcpu_handle)
{
    struct vmx_vm* vm = lookup_vm(vm_handle);
    struct vmx_vcpu* vcpu = NULL;
    u32 gen;
    int i;

    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;
    if (vm->vcpu) return EBUSY; /* one vCPU per VM */

    for (i = 0; i < VMX_MAX_VCPUS; i++) {
        if (!vcpu_table[i].in_use) {
            vcpu = &vcpu_table[i];
            break;
        }
    }
    if (!vcpu) return ENOSPC;

    gen = vcpu->gen + 1;
    memset(vcpu, 0, sizeof(*vcpu));
    vcpu->gen = gen & 0xffffff;
    if (vcpu->gen == 0) vcpu->gen = 1;
    vcpu->vmcs_phys = __pa(vmcs_regions[i]);

    vcpu->vm = vm;
    vcpu->owner = owner;
    vcpu->in_use = 1;

    if (vmx_vmcs_setup(vcpu) != 0) {
        vcpu->in_use = 0;
        return EIO;
    }

    vm->vcpu = vcpu;
    *vcpu_handle = HANDLE(vcpu->gen, i);
    return 0;
}

int vmx_vcpu_destroy(endpoint_t owner, u32 handle)
{
    struct vmx_vcpu* vcpu = lookup_vcpu(handle);

    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (vcpu->running) return EBUSY;

    vcpu->vm->vcpu = NULL;
    destroy_vcpu_internal(vcpu);
    return 0;
}

/*****************************************************************************
 *                        guest state
 *****************************************************************************/

/* fxsave image layout conversion */

static void fpu_record_to_image(u8 img[512], const struct vmx_guest_fpu* f)
{
    memset(img, 0, 512);
    *(u16*)(img + 0) = f->cwd;
    *(u16*)(img + 2) = f->swd;
    img[4] = f->ftw;
    *(u16*)(img + 6) = f->fop;
    *(u64*)(img + 8) = f->fip;
    *(u64*)(img + 16) = f->fdp;
    *(u32*)(img + 24) = f->mxcsr;
    *(u32*)(img + 28) = f->mxcsr_mask;
    memcpy(img + 32, f->st, sizeof(f->st));
    memcpy(img + 160, f->xmm, sizeof(f->xmm));
}

static void image_to_fpu_record(struct vmx_guest_fpu* f, const u8 img[512])
{
    memset(f, 0, sizeof(*f));
    f->cwd = *(u16*)(img + 0);
    f->swd = *(u16*)(img + 2);
    f->ftw = img[4];
    f->fop = *(u16*)(img + 6);
    f->fip = *(u64*)(img + 8);
    f->fdp = *(u64*)(img + 16);
    f->mxcsr = *(u32*)(img + 24);
    f->mxcsr_mask = vmx_state.mxcsr_mask;
    memcpy(f->st, img + 32, sizeof(f->st));
    memcpy(f->xmm, img + 160, sizeof(f->xmm));
}

static inline void fxsave_image(u8 img[512])
{
    __asm__ __volatile__("fxsave64 %0" : "=m"(*(u8(*)[512])img)::"memory");
}

static inline void fxrstor_image(const u8 img[512])
{
    __asm__ __volatile__("fxrstor64 %0" ::"m"(*(const u8(*)[512])img)
                         : "memory");
}

int vmx_vcpu_set_state(endpoint_t owner, u32 handle,
                       const struct vmx_guest_state* st)
{
    struct vmx_vcpu* vcpu = lookup_vcpu(handle);
    int retval;

    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (vcpu->running) return EBUSY;

    if ((retval = vmx_validate_state(st))) return retval;

    if (vmx_state.current_vmcs != vcpu->vmcs_phys) {
        if (vmx_vmcs_load(vcpu->vmcs_phys)) return EIO;
        vmx_state.current_vmcs = vcpu->vmcs_phys;
    }

    if ((retval = vmx_vmcs_write_state(vcpu, st))) return retval;

    memcpy(vcpu->gprs, st->gprs, sizeof(vcpu->gprs));
    vcpu->fpu = st->fpu;
    vcpu->fpu.mxcsr_mask = vmx_state.mxcsr_mask;
    vcpu->state_valid = 1;

    return 0;
}

int vmx_vcpu_get_state(endpoint_t owner, u32 handle, struct vmx_guest_state* st)
{
    struct vmx_vcpu* vcpu = lookup_vcpu(handle);

    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (!vcpu->state_valid) return ENODATA;

    if (vmx_state.current_vmcs != vcpu->vmcs_phys) {
        if (vmx_vmcs_load(vcpu->vmcs_phys)) return EIO;
        vmx_state.current_vmcs = vcpu->vmcs_phys;
    }

    vmx_vmcs_read_state(vcpu, st);
    return 0;
}

/*****************************************************************************
 *                        guest execution
 *****************************************************************************/

int vmx_exit_dispatch(struct vmx_vcpu* vcpu)
{
    u32 reason = vmx_vmread(VMX_EXIT_REASON) & 0xffff;

    if (reason == VMX_EXIT_EXTERNAL_INTERRUPT) {
        u32 info = vmx_vmread(VMX_EXIT_INTR_INFO);

        if (info & INTR_INFO_VALID) {
            unsigned type =
                (info >> INTR_INFO_TYPE_SHIFT) & INTR_INFO_TYPE_MASK;
            unsigned vec = info & INTR_INFO_VECTOR_MASK;

            if (type == INTR_TYPE_EXT_INTR) {
#if CONFIG_X86_LOCAL_APIC
                if (vec == APIC_TIMER_INT_VECTOR) {
                    apic_timer_int_handler();
                    apic_eoi();
                    return 0;
                }
#endif
                if (vec >= INT_VECTOR_IRQ0 && vec < INT_VECTOR_IRQ0 + 64) {
                    generic_handle_irq(vec - INT_VECTOR_IRQ0);
                }
                /* other vectors (spurious etc.) are ignored */
            }
            /* NMIs are reported to userspace as host interruption */
        }
    }

    return 0;
}

static int vmx_fill_exit_record(struct vmx_vcpu* vcpu, struct vmx_exit* e)
{
    u64 raw_reason = vmx_vmread(VMX_EXIT_REASON);
    u32 reason = raw_reason & 0xffff;
    u32 insn_len;

    memset(e, 0, sizeof(*e));

    e->reason = reason;
    e->entry_failure = (raw_reason >> 31) & 1;
    e->qualification = vmx_vmread(VMX_EXIT_QUALIFICATION);

    insn_len = (u32)vmx_vmread(VMX_EXIT_INSN_LEN);
    e->insn_len = insn_len;
    if (insn_len) e->flags |= VMX_EXIT_F_INSN_LEN;

    if (reason == VMX_EXIT_EPT_VIOLATION ||
        reason == VMX_EXIT_EPT_MISCONFIGURATION) {
        e->gpa = vmx_vmread(VMX_GUEST_PHYSICAL_ADDR);
        e->flags |= VMX_EXIT_F_GPA;
    }
    if (reason == VMX_EXIT_EPT_VIOLATION &&
        (e->qualification & VMX_EPT_VIOL_GLA_VALID)) {
        e->gla = vmx_vmread(VMX_GUEST_LINEAR_ADDR);
        e->flags |= VMX_EXIT_F_GLA;
    }

    e->intr_info = (u32)vmx_vmread(VMX_EXIT_INTR_INFO);
    e->intr_error = (u32)vmx_vmread(VMX_EXIT_INTR_ERROR_CODE);
    e->idt_vectoring = (u32)vmx_vmread(VMX_IDT_VECTORING_INFO);

    e->rip = vmx_vmread(VMX_GUEST_RIP);
    e->rflags = vmx_vmread(VMX_GUEST_RFLAGS);

    /* VMCS launch state only advances on a real guest entry */
    if (!e->entry_failure) vcpu->launched = 1;

    return 0;
}

int vmx_vcpu_run(endpoint_t owner, u32 handle, struct vmx_exit* exit_out)
{
    static u8 fpu_image[512] __attribute__((aligned(16)));
    struct vmx_vcpu* vcpu = lookup_vcpu(handle);
    struct proc* p = endpt_proc(owner);
    struct proc* fpu_owner_proc;
    int retval;

    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (!vcpu->state_valid) return ENODATA;
    if (vcpu->running) return EBUSY;
    if (!p) return EINVAL;

    if (vmx_state.current_vmcs != vcpu->vmcs_phys) {
        if (vmx_vmcs_load(vcpu->vmcs_phys)) return EIO;
        vmx_state.current_vmcs = vcpu->vmcs_phys;
    }
    if ((retval = vmx_vmcs_refresh_host(vcpu))) {
        printk("vmx: run failed: host-state VMWRITE, error %u\n",
               (u32)vmx_vmread(VMX_INSTRUCTION_ERROR));
        return EIO;
    }

    vcpu->running = 1;
    vcpu->vm->ever_ran = 1;
    vmx_current_vcpu = vcpu;

    /*
     * FPU handoff: save the lazy owner's state and make the host lazy-FPU
     * machinery dormant (owner NULL, TS set) while the guest owns the unit.
     */
    fpu_owner_proc = get_cpulocal_var(fpu_owner);
    if (fpu_owner_proc) {
        disable_fpu_exception();
        save_local_fpu(fpu_owner_proc, TRUE);
        get_cpulocal_var(fpu_owner) = NULL;
    }

    disable_fpu_exception(); /* clts: fxrstor needs the unit enabled */
    fpu_record_to_image(fpu_image, &vcpu->fpu);
    fxrstor_image(fpu_image);

    retval = vmx_enter_guest(vcpu);

    /*
     * The guest no longer owns the FPU.  A reschedule while the guest was
     * running goes through the lazy-FPU switch path, which re-arms CR0.TS
     * (fpu_owner is NULL here), so clear it again before saving.
     */
    disable_fpu_exception();
    fxsave_image(fpu_image);
    image_to_fpu_record(&vcpu->fpu, fpu_image);
    enable_fpu_exception();

    vcpu->running = 0;
    vmx_current_vcpu = NULL;

    /* charge guest time to the bound thread */
    stop_context(p);

    if (retval != 0) {
        /* VMLAUNCH/VMRESUME instruction failure */
        if (retval == -2) {
            printk("vmx: run failed: VMfailInvalid (no valid current VMCS)\n");
        } else {
            vcpu->entry_error = (u32)vmx_vmread(VMX_INSTRUCTION_ERROR);
            printk("vmx: run failed: %s VMfailValid, instruction error %u\n",
                   vcpu->launched ? "VMRESUME" : "VMLAUNCH", vcpu->entry_error);
            if (vcpu->entry_error == 7) {
                printk(
                    "vmx: controls pin=%x proc=%x proc2=%x exit=%x entry=%x\n",
                    (u32)vmx_vmread(VMX_PINBASED_EXEC_CTL),
                    (u32)vmx_vmread(VMX_PROCBASED_EXEC_CTL),
                    (u32)vmx_vmread(VMX_PROCBASED_EXEC_CTL2),
                    (u32)vmx_vmread(VMX_VM_EXIT_CTLS),
                    (u32)vmx_vmread(VMX_VM_ENTRY_CTLS));
            }
        }
        return EIO;
    }

    return vmx_fill_exit_record(vcpu, exit_out);
}

/*****************************************************************************
 *                        owner death cleanup
 *****************************************************************************/

void vmx_proc_cleanup(struct proc* p)
{
    int i;

    for (i = 0; i < VMX_MAX_VMS; i++) {
        struct vmx_vm* vm = &vm_table[i];
        if (vm->in_use && vm->owner == p->endpoint) {
            vmx_vm_destroy(p->endpoint, HANDLE(vm->gen, i));
        }
    }
    /* vCPUs bound to this proc but no longer owned by a live VM */
    for (i = 0; i < VMX_MAX_VCPUS; i++) {
        struct vmx_vcpu* vcpu = &vcpu_table[i];
        if (vcpu->in_use && vcpu->owner == p->endpoint && !vcpu->vm->in_use) {
            destroy_vcpu_internal(vcpu);
        }
    }
}

/* Opaque transport adapters: the VMX ABI is decoded only inside this backend.
 */
static int vmx_set_state(endpoint_t owner, u32 handle, const void* state)
{
    return vmx_vcpu_set_state(owner, handle, state);
}

static int vmx_get_state(endpoint_t owner, u32 handle, void* state)
{
    return vmx_vcpu_get_state(owner, handle, state);
}

static int vmx_run(endpoint_t owner, u32 handle, void* exit)
{
    return vmx_vcpu_run(owner, handle, exit);
}

static const struct hv_backend_ops vmx_backend_ops = {
    .backend = HV_BACKEND_VMX,
    .state_size = sizeof(struct vmx_guest_state),
    .exit_size = sizeof(struct vmx_exit),
    .query_caps = vmx_query_caps,
    .vm_create = vmx_vm_create,
    .vm_destroy = vmx_vm_destroy,
    .vm_set_memslot = vmx_vm_set_memslot,
    .vcpu_create = vmx_vcpu_create,
    .vcpu_destroy = vmx_vcpu_destroy,
    .vcpu_set_state = vmx_set_state,
    .vcpu_get_state = vmx_get_state,
    .vcpu_run = vmx_run,
    .proc_cleanup = vmx_proc_cleanup,
};
