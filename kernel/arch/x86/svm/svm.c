/* AMD SVM implementation of the Lyos hardware-virtualization backend. */
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/vm.h>
#include <errno.h>
#include <string.h>
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <asm/proto.h>
#include <asm/const.h>
#include <asm/cpu_info.h>
#include <asm/page.h>
#include <asm/smp.h>
#include "svm.h"

extern struct cpu_info cpu_info[CONFIG_SMP_MAX_CPUS];
extern void _cpuid(u32* eax, u32* ebx, u32* ecx, u32* edx);

struct svm_vcpu* svm_current_vcpu;

static int svm_active;
static u32 svm_caps;
static u32 svm_mxcsr_mask;
static u8 hsave_page[ARCH_PG_SIZE] __attribute__((aligned(ARCH_PG_SIZE)));
static struct svm_vmcb vmcb_pages[SVM_MAX_VCPUS];
static u8 iopm[3 * ARCH_PG_SIZE] __attribute__((aligned(ARCH_PG_SIZE)));
static u8 msrpm[2 * ARCH_PG_SIZE] __attribute__((aligned(ARCH_PG_SIZE)));
static struct svm_vm vm_table[SVM_MAX_VMS];
static struct svm_vcpu vcpu_table[SVM_MAX_VCPUS];
static const struct hv_backend_ops svm_backend_ops;

#define HANDLE(gen, idx) ((((u32)(gen)) << 8) | (u32)(idx))
#define HANDLE_IDX(h)    ((h) & 0xff)
#define HANDLE_GEN(h)    (((h) >> 8) & 0xffffff)

static u64 read_msr(u32 msr)
{
    u32 hi, lo;
    ia32_read_msr(msr, &hi, &lo);
    return make64(hi, lo);
}

static void write_msr(u32 msr, u64 value)
{
    ia32_write_msr(msr, (u32)(value >> 32), (u32)value);
}

static void probe_mxcsr_mask(void)
{
    static u8 image[512] __attribute__((aligned(16)));
    unsigned long cr0 = read_cr0();
    memset(image, 0, sizeof(image));
    disable_fpu_exception();
    __asm__ __volatile__("fxsave64 %0" : "=m"(image));
    write_cr0(cr0);
    svm_mxcsr_mask = *(u32*)(image + 28);
    if (!svm_mxcsr_mask) svm_mxcsr_mask = 0xffbf;
}

void svm_early_init(void)
{
    u32 eax, ebx, ecx, edx;
    u64 efer;

    memset(vm_table, 0, sizeof(vm_table));
    memset(vcpu_table, 0, sizeof(vcpu_table));
    if (cpu_info[cpuid].vendor != CPU_VENDOR_AMD) return;

    eax = 0x80000000;
    _cpuid(&eax, &ebx, &ecx, &edx);
    if (eax < 0x8000000a) {
        printk("svm: unavailable: extended CPUID leaves are missing\n");
        return;
    }
    eax = 0x80000001;
    _cpuid(&eax, &ebx, &ecx, &edx);
    if (!(ecx & SVM_CPUID_FEATURE)) {
        printk("svm: unavailable: CPUID does not expose SVM\n");
        return;
    }
    if (read_msr(MSR_VM_CR) & VM_CR_SVMDIS) {
        printk("svm: unavailable: firmware disabled SVM\n");
        return;
    }
    eax = 0x8000000a;
    _cpuid(&eax, &ebx, &ecx, &edx);
    if (!(edx & SVM_FEATURE_NPT) || !(edx & SVM_FEATURE_NRIPS)) {
        printk("svm: unavailable: nested paging or NRIP save is missing\n");
        return;
    }

    memset(iopm, 0xff, sizeof(iopm));
    memset(msrpm, 0xff, sizeof(msrpm));
    memset(hsave_page, 0, sizeof(hsave_page));
    write_msr(MSR_VM_HSAVE_PA, __pa(hsave_page));
    efer = read_msr(AMD_MSR_EFER);
    write_msr(AMD_MSR_EFER, efer | EFER_SVME);
    if (!(read_msr(AMD_MSR_EFER) & EFER_SVME)) {
        printk("svm: unavailable: could not enable EFER.SVME\n");
        return;
    }

    probe_mxcsr_mask();
    svm_caps = HV_CAP_STAGE2 | HV_CAP_X86_32 | HV_CAP_ACTIVE;
    svm_active = 1;
    if (hv_backend_register(&svm_backend_ops))
        panic("svm: cannot register hypervisor backend");
    printk("svm: AMD-V active, nested paging and NRIP save available\n");
}

static struct svm_vm* lookup_vm(u32 handle)
{
    u32 i = HANDLE_IDX(handle);
    struct svm_vm* vm;
    if (i >= SVM_MAX_VMS) return NULL;
    vm = &vm_table[i];
    return vm->in_use && vm->gen == HANDLE_GEN(handle) ? vm : NULL;
}

static struct svm_vcpu* lookup_vcpu(u32 handle)
{
    u32 i = HANDLE_IDX(handle);
    struct svm_vcpu* vcpu;
    if (i >= SVM_MAX_VCPUS) return NULL;
    vcpu = &vcpu_table[i];
    return vcpu->in_use && vcpu->gen == HANDLE_GEN(handle) ? vcpu : NULL;
}

static int svm_query_caps(struct hv_caps* caps)
{
    int i;
    memset(caps, 0, sizeof(*caps));
    caps->abi_version = HV_ABI_VERSION;
    caps->caps = svm_caps;
    caps->backend = HV_BACKEND_SVM;
    caps->translation_pages = svm_npt_pages_in_use();
    for (i = 0; i < SVM_MAX_VMS; i++)
        caps->active_vms += vm_table[i].in_use;
    for (i = 0; i < SVM_MAX_VCPUS; i++)
        caps->active_vcpus += vcpu_table[i].in_use;
    return 0;
}

static int svm_vm_create(endpoint_t owner, u32* handle)
{
    struct svm_vm* vm = NULL;
    u32 gen;
    int i, retval;
    if (!svm_active) return EOPNOTSUPP;
    for (i = 0; i < SVM_MAX_VMS; i++)
        if (!vm_table[i].in_use) {
            vm = &vm_table[i];
            break;
        }
    if (!vm) return ENOSPC;
    gen = vm->gen + 1;
    memset(vm, 0, sizeof(*vm));
    vm->gen = gen & 0xffffff;
    if (!vm->gen) vm->gen = 1;
    retval = svm_npt_init_vm(vm);
    if (retval) return retval;
    vm->owner = owner;
    vm->in_use = 1;
    *handle = HANDLE(vm->gen, i);
    return 0;
}

static void destroy_vcpu(struct svm_vcpu* vcpu)
{
    memset(__va(vcpu->vmcb_phys), 0, ARCH_PG_SIZE);
    vcpu->in_use = 0;
    vcpu->gen = (vcpu->gen + 1) & 0xffffff;
    if (!vcpu->gen) vcpu->gen = 1;
}

static int svm_vm_destroy(endpoint_t owner, u32 handle)
{
    struct svm_vm* vm = lookup_vm(handle);
    int i;
    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;
    if (vm->vcpu) {
        if (vm->vcpu->running) return EBUSY;
        destroy_vcpu(vm->vcpu);
    }
    svm_npt_destroy_vm(vm);
    for (i = 0; i < SVM_MAX_MEMSLOTS; i++) {
        if (vm->slots[i].in_use) {
            hv_backing_put(vm->slots[i].backing);
            vm->slots[i].in_use = 0;
        }
    }
    vm->in_use = 0;
    vm->gen = (vm->gen + 1) & 0xffffff;
    if (!vm->gen) vm->gen = 1;
    return 0;
}

static int svm_vm_set_memslot(endpoint_t owner, u32 handle,
                              const struct hv_memslot* slot)
{
    struct svm_vm* vm = lookup_vm(handle);
    struct hv_backing* backing;
    unsigned long i, first, npages, slot_idx;
    u64 end;
    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;
    if (vm->ever_ran) return EPERM;
    if ((slot->gpa | slot->len | slot->offset) & (ARCH_PG_SIZE - 1))
        return EINVAL;
    if (!slot->len || !(slot->flags & HV_MEM_READ) ||
        (slot->flags & ~HV_MEM_ALL))
        return EINVAL;
    end = slot->gpa + slot->len;
    if (end < slot->gpa || end > (1ULL << 48)) return EINVAL;
    backing = hv_backing_lookup(slot->mm_handle);
    if (!backing) return ENOENT;
    if (slot->offset > backing->npages * ARCH_PG_SIZE ||
        slot->len > backing->npages * ARCH_PG_SIZE - slot->offset)
        return EINVAL;
    for (i = 0; i < SVM_MAX_MEMSLOTS; i++) {
        struct svm_memslot* s = &vm->slots[i];
        if (s->in_use && slot->gpa < s->gpa + s->len && s->gpa < end)
            return EBUSY;
    }
    for (slot_idx = 0; slot_idx < SVM_MAX_MEMSLOTS; slot_idx++)
        if (!vm->slots[slot_idx].in_use) break;
    if (slot_idx == SVM_MAX_MEMSLOTS) return ENOSPC;
    npages = slot->len >> ARCH_PG_SHIFT;
    if (svm_npt_pages_in_use() + npages / 512 + 4 > SVM_NPT_POOL_PAGES)
        return ENOMEM;
    first = slot->offset >> ARCH_PG_SHIFT;
    for (i = 0; i < npages; i++) {
        int r =
            svm_npt_map_page(vm, slot->gpa + (i << ARCH_PG_SHIFT),
                             hv_backing_page(backing, first + i), slot->flags);
        if (r) {
            vm->ever_ran = 1;
            return r;
        }
    }
    vm->slots[slot_idx].in_use = 1;
    vm->slots[slot_idx].gpa = slot->gpa;
    vm->slots[slot_idx].len = slot->len;
    vm->slots[slot_idx].backing = backing;
    hv_backing_get(backing);
    svm_npt_invalidate(vm);
    return 0;
}

static u16 svm_attrib(u32 ar) { return (ar & 0xff) | ((ar >> 4) & 0xf00); }

static u32 vmx_attrib(u16 attr)
{
    return (attr & 0xff) | ((u32)(attr & 0xf00) << 4);
}

static void write_segment(struct svm_segment* d, const struct vmx_segment* s)
{
    d->selector = s->selector;
    d->attrib = svm_attrib(s->ar);
    d->limit = s->limit;
    d->base = s->base;
}

static void read_segment(struct vmx_segment* d, const struct svm_segment* s)
{
    memset(d, 0, sizeof(*d));
    d->selector = s->selector;
    d->ar = vmx_attrib(s->attrib);
    d->limit = s->limit;
    d->base = s->base;
}

static int validate_segment(const struct vmx_segment* s, int code, int stack)
{
    u32 type = s->ar & VMX_SEG_TYPE_MASK;
    if (s->selector & 7) return EINVAL;
    if (s->ar & ~VMX_SEG_AR_MASK) return EINVAL;
    if (!(s->ar & VMX_SEG_P) || !(s->ar & VMX_SEG_S)) return EINVAL;
    if (code) {
        if (type != 0xb || (s->ar & VMX_SEG_L)) return EINVAL;
    } else if ((type & 7) != 3 && type != 1)
        return EINVAL;
    if (stack && type != 3) return EINVAL;
    if ((s->ar & VMX_SEG_G) && (s->limit & 0xfff) != 0xfff) return EINVAL;
    return 0;
}

static int validate_state(const struct vmx_guest_state* st)
{
    int i;
    if (st->version != VMX_STATE_VERSION || st->size != sizeof(*st))
        return EINVAL;
    if (st->flags || st->reserved0 || st->reserved1[0] || st->reserved1[1])
        return EINVAL;
    if (st->efer || st->dr7 != 0x400 ||
        st->activity_state != VMX_ACTIVITY_ACTIVE || st->interruptibility)
        return EINVAL;
    if (!(st->rflags & 2) || st->rflags >> 32 || (st->rflags & (1UL << 17)))
        return EINVAL;
    if (!(st->cr0 & 1) ||
        (st->cr0 & ((1UL << 31) | (1UL << 29) | (1UL << 3) | (1UL << 2))))
        return EINVAL;
    if ((st->cr4 >> 32) ||
        (st->cr4 & ((1UL << 5) | (1UL << 13) | (1UL << 17) | (1UL << 18))) ||
        !(st->cr4 & (1UL << 9)))
        return EINVAL;
    if (st->fpu.mxcsr & ~svm_mxcsr_mask) return EINVAL;
    for (i = 0; i < VMX_NR_GPRS; i++)
        if (st->gprs[i] >> 32) return EINVAL;
    if (st->rip >> 32) return EINVAL;
    if (validate_segment(&st->cs, 1, 0) || validate_segment(&st->ss, 0, 1) ||
        validate_segment(&st->ds, 0, 0) || validate_segment(&st->es, 0, 0) ||
        validate_segment(&st->fs, 0, 0) || validate_segment(&st->gs, 0, 0))
        return EINVAL;
    return 0;
}

static void setup_vmcb(struct svm_vcpu* vcpu)
{
    struct svm_vmcb* v = __va(vcpu->vmcb_phys);
    memset(v, 0, sizeof(*v));
    v->control.intercept_cr_read =
        (1U << 0) | (1U << 3) | (1U << 4) | (1U << 8);
    v->control.intercept_cr_write = v->control.intercept_cr_read;
    /* Only DR0-DR7 have intercept bits; the upper half is reserved. */
    v->control.intercept_dr_read = 0x00ff;
    v->control.intercept_dr_write = 0x00ff;
    /* Architectural exception vectors 0-14 and 16-19; vector 15 and the
     * remaining positions are reserved on the baseline SVM interface. */
    v->control.intercept_exceptions = 0x000f7fff;
    v->control.intercept =
        SVM_INTERCEPT_INTR | SVM_INTERCEPT_NMI | SVM_INTERCEPT_IDTR_READ |
        SVM_INTERCEPT_GDTR_READ | SVM_INTERCEPT_LDTR_READ |
        SVM_INTERCEPT_TR_READ | SVM_INTERCEPT_IDTR_WRITE |
        SVM_INTERCEPT_GDTR_WRITE | SVM_INTERCEPT_LDTR_WRITE |
        SVM_INTERCEPT_TR_WRITE | SVM_INTERCEPT_CPUID | SVM_INTERCEPT_INVD |
        SVM_INTERCEPT_PAUSE | SVM_INTERCEPT_HLT | SVM_INTERCEPT_INVLPG |
        SVM_INTERCEPT_INVLPGA | SVM_INTERCEPT_IOIO | SVM_INTERCEPT_MSR |
        SVM_INTERCEPT_SHUTDOWN | SVM_INTERCEPT_VMRUN | SVM_INTERCEPT_VMMCALL |
        SVM_INTERCEPT_VMLOAD | SVM_INTERCEPT_VMSAVE | SVM_INTERCEPT_STGI |
        SVM_INTERCEPT_CLGI | SVM_INTERCEPT_SKINIT | SVM_INTERCEPT_RDTSCP |
        SVM_INTERCEPT_WBINVD | SVM_INTERCEPT_MONITOR | SVM_INTERCEPT_MWAIT |
        SVM_INTERCEPT_XSETBV;
    v->control.iopm_base_pa = __pa(iopm);
    v->control.msrpm_base_pa = __pa(msrpm);
    v->control.asid = 1;
    v->control.tlb_ctl = 1;
    v->control.int_ctl = SVM_INT_CTL_V_INTR_MASKING;
    v->control.np_enable = SVM_NESTED_CTL_NP_ENABLE;
    v->control.n_cr3 = vcpu->vm->ncr3;
    v->control.clean = 0;
    v->save.g_pat = 0x0007040600070406ULL;
    v->save.dr6 = 0xffff0ff0;
}

static int svm_vcpu_create(endpoint_t owner, u32 vm_handle, u32* handle)
{
    struct svm_vm* vm = lookup_vm(vm_handle);
    struct svm_vcpu* vcpu = NULL;
    u32 gen;
    int i;
    if (!vm) return EINVAL;
    if (vm->owner != owner) return EPERM;
    if (vm->vcpu) return EBUSY;
    for (i = 0; i < SVM_MAX_VCPUS; i++)
        if (!vcpu_table[i].in_use) {
            vcpu = &vcpu_table[i];
            break;
        }
    if (!vcpu) return ENOSPC;
    gen = vcpu->gen + 1;
    memset(vcpu, 0, sizeof(*vcpu));
    vcpu->gen = gen & 0xffffff;
    if (!vcpu->gen) vcpu->gen = 1;
    vcpu->vmcb_phys = __pa(&vmcb_pages[i]);
    vcpu->vm = vm;
    vcpu->owner = owner;
    vcpu->in_use = 1;
    setup_vmcb(vcpu);
    vm->vcpu = vcpu;
    *handle = HANDLE(vcpu->gen, i);
    return 0;
}

static int svm_vcpu_destroy(endpoint_t owner, u32 handle)
{
    struct svm_vcpu* vcpu = lookup_vcpu(handle);
    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (vcpu->running) return EBUSY;
    vcpu->vm->vcpu = NULL;
    destroy_vcpu(vcpu);
    return 0;
}

static void state_to_vmcb(struct svm_vcpu* vcpu,
                          const struct vmx_guest_state* st)
{
    struct svm_vmcb* v = __va(vcpu->vmcb_phys);
    memcpy(vcpu->gprs, st->gprs, sizeof(vcpu->gprs));
    vcpu->fpu = st->fpu;
    vcpu->fpu.mxcsr_mask = svm_mxcsr_mask;
    write_segment(&v->save.cs, &st->cs);
    write_segment(&v->save.ss, &st->ss);
    write_segment(&v->save.ds, &st->ds);
    write_segment(&v->save.es, &st->es);
    write_segment(&v->save.fs, &st->fs);
    write_segment(&v->save.gs, &st->gs);
    v->save.gdtr.base = st->gdtr.base;
    v->save.gdtr.limit = st->gdtr.limit;
    v->save.idtr.base = st->idtr.base;
    v->save.idtr.limit = st->idtr.limit;
    v->save.ldtr.attrib = 0;
    v->save.tr.attrib = 0x8b;
    v->save.tr.selector = 0x18;
    v->save.tr.limit = 0xffff;
    v->save.cr0 = st->cr0;
    v->save.cr3 = st->cr3;
    v->save.cr4 = st->cr4;
    /* SVM requires SVME in the hardware guest EFER.  It remains hidden from
     * the constrained guest ABI, which exposes EFER as zero. */
    v->save.efer = st->efer | EFER_SVME;
    v->save.dr7 = st->dr7;
    v->save.rflags = st->rflags;
    v->save.rip = st->rip;
    v->save.rsp = st->gprs[VMX_GPR_RSP];
    v->save.rax = st->gprs[VMX_GPR_RAX];
    v->save.cpl = st->cs.selector & 3;
    v->control.clean = 0;
}

static void vmcb_to_state(struct svm_vcpu* vcpu, struct vmx_guest_state* st)
{
    struct svm_vmcb* v = __va(vcpu->vmcb_phys);
    memset(st, 0, sizeof(*st));
    st->version = VMX_STATE_VERSION;
    st->size = sizeof(*st);
    memcpy(st->gprs, vcpu->gprs, sizeof(st->gprs));
    st->gprs[VMX_GPR_RAX] = v->save.rax;
    st->gprs[VMX_GPR_RSP] = v->save.rsp;
    st->fpu = vcpu->fpu;
    st->rip = v->save.rip;
    st->rflags = v->save.rflags;
    st->cr0 = v->save.cr0;
    st->cr3 = v->save.cr3;
    st->cr4 = v->save.cr4;
    st->efer = v->save.efer & ~EFER_SVME;
    st->dr7 = v->save.dr7;
    read_segment(&st->cs, &v->save.cs);
    read_segment(&st->ss, &v->save.ss);
    read_segment(&st->ds, &v->save.ds);
    read_segment(&st->es, &v->save.es);
    read_segment(&st->fs, &v->save.fs);
    read_segment(&st->gs, &v->save.gs);
    st->gdtr.base = v->save.gdtr.base;
    st->gdtr.limit = v->save.gdtr.limit;
    st->idtr.base = v->save.idtr.base;
    st->idtr.limit = v->save.idtr.limit;
    st->activity_state = VMX_ACTIVITY_ACTIVE;
}

static int svm_set_state(endpoint_t owner, u32 handle, const void* state)
{
    struct svm_vcpu* vcpu = lookup_vcpu(handle);
    const struct vmx_guest_state* st = state;
    int retval;
    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (vcpu->running) return EBUSY;
    if ((retval = validate_state(st))) return retval;
    state_to_vmcb(vcpu, st);
    vcpu->state_valid = 1;
    return 0;
}

static int svm_get_state(endpoint_t owner, u32 handle, void* state)
{
    struct svm_vcpu* vcpu = lookup_vcpu(handle);
    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (!vcpu->state_valid) return ENODATA;
    vmcb_to_state(vcpu, state);
    return 0;
}

static void fpu_to_image(u8 img[512], const struct vmx_guest_fpu* f)
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

static void image_to_fpu(struct vmx_guest_fpu* f, const u8 img[512])
{
    memset(f, 0, sizeof(*f));
    f->cwd = *(u16*)(img + 0);
    f->swd = *(u16*)(img + 2);
    f->ftw = img[4];
    f->fop = *(u16*)(img + 6);
    f->fip = *(u64*)(img + 8);
    f->fdp = *(u64*)(img + 16);
    f->mxcsr = *(u32*)(img + 24);
    f->mxcsr_mask = svm_mxcsr_mask;
    memcpy(f->st, img + 32, sizeof(f->st));
    memcpy(f->xmm, img + 160, sizeof(f->xmm));
}

static u32 translate_exit(u64 code, u64 info1)
{
    if (code >= SVM_EXIT_READ_CR0 && code < SVM_EXIT_READ_CR0 + 16)
        return VMX_EXIT_CR_ACCESS;
    if (code >= SVM_EXIT_WRITE_CR0 && code < SVM_EXIT_WRITE_CR0 + 16)
        return VMX_EXIT_CR_ACCESS;
    if (code >= SVM_EXIT_READ_DR0 && code < SVM_EXIT_READ_DR0 + 16)
        return VMX_EXIT_DR_ACCESS;
    if (code >= SVM_EXIT_WRITE_DR0 && code < SVM_EXIT_WRITE_DR0 + 16)
        return VMX_EXIT_DR_ACCESS;
    if (code >= SVM_EXIT_EXCP_BASE && code < SVM_EXIT_EXCP_BASE + 32)
        return VMX_EXIT_EXCEPTION;
    switch (code) {
    case SVM_EXIT_INTR:
    case SVM_EXIT_NMI:
        return VMX_EXIT_EXTERNAL_INTERRUPT;
    case SVM_EXIT_SHUTDOWN:
        return VMX_EXIT_TRIPLE_FAULT;
    case SVM_EXIT_CPUID:
        return VMX_EXIT_CPUID;
    case SVM_EXIT_HLT:
        return VMX_EXIT_HLT;
    case SVM_EXIT_INVLPG:
        return VMX_EXIT_INVLPG;
    case SVM_EXIT_IOIO:
        return VMX_EXIT_IO_INSTRUCTION;
    case SVM_EXIT_MSR:
        return info1 ? VMX_EXIT_WRMSR : VMX_EXIT_RDMSR;
    case SVM_EXIT_PAUSE:
        return VMX_EXIT_PAUSE;
    case SVM_EXIT_WBINVD:
        return VMX_EXIT_WBINVD;
    case SVM_EXIT_XSETBV:
        return VMX_EXIT_XSETBV;
    case SVM_EXIT_NPF:
        return VMX_EXIT_EPT_VIOLATION;
    default:
        if (code >= 0x66 && code <= 0x6d)
            return code == 0x66 || code == 0x67 || code == 0x6a || code == 0x6b
                       ? VMX_EXIT_GDTR_IDTR_ACCESS
                       : VMX_EXIT_LDTR_TR_ACCESS;
        return (u32)code;
    }
}

static u64 io_qualification(u64 info)
{
    unsigned bytes = (info >> 4) & 7;
    unsigned size = bytes ? bytes - 1 : 0;
    return (((info >> 16) & 0xffff) << 16) | size |
           ((info & 1) ? (1ULL << 3) : 0) |
           ((info & (1ULL << 2)) ? (1ULL << 4) : 0);
}

static void fill_exit(struct svm_vcpu* vcpu, struct vmx_exit* e)
{
    struct svm_vmcb* v = __va(vcpu->vmcb_phys);
    u64 code = v->control.exit_code;
    memset(e, 0, sizeof(*e));
    e->reason = translate_exit(code, v->control.exit_info1);
    e->qualification = code == SVM_EXIT_IOIO
                           ? io_qualification(v->control.exit_info1)
                           : v->control.exit_info1;
    e->intr_info = (u32)v->control.exit_int_info;
    e->intr_error = (u32)(v->control.exit_int_info >> 32);
    e->rip = v->save.rip;
    e->rflags = v->save.rflags;
    if (v->control.nrip >= v->save.rip && v->control.nrip - v->save.rip <= 15) {
        e->insn_len = v->control.nrip - v->save.rip;
        if (e->insn_len) e->flags |= VMX_EXIT_F_INSN_LEN;
    }
    if (code == SVM_EXIT_NPF) {
        u64 info = v->control.exit_info1;
        e->gpa = v->control.exit_info2;
        e->flags |= VMX_EXIT_F_GPA;
        e->qualification = (info & (1ULL << 1))   ? VMX_EPT_VIOL_WRITE
                           : (info & (1ULL << 4)) ? VMX_EPT_VIOL_FETCH
                                                  : VMX_EPT_VIOL_READ;
    }
    if (code == ~0ULL) e->entry_failure = 1;
}

static int svm_run(endpoint_t owner, u32 handle, void* exit_out)
{
    static u8 image[512] __attribute__((aligned(16)));
    struct svm_vcpu* vcpu = lookup_vcpu(handle);
    struct svm_vmcb* vmcb;
    struct proc* p = endpt_proc(owner);
    struct proc* fpu_owner_proc;
    if (!vcpu) return EINVAL;
    if (vcpu->owner != owner) return EPERM;
    if (!vcpu->state_valid) return ENODATA;
    if (vcpu->running) return EBUSY;
    if (!p) return EINVAL;
    vmcb = __va(vcpu->vmcb_phys);
    vmcb->control.clean = 0;
    /* One ASID is shared by the static vCPU pool; flush before every entry. */
    vmcb->control.tlb_ctl = 1;
    vmcb->save.rax = vcpu->gprs[VMX_GPR_RAX];
    vcpu->running = 1;
    vcpu->vm->ever_ran = 1;
    svm_current_vcpu = vcpu;
    fpu_owner_proc = get_cpulocal_var(fpu_owner);
    if (fpu_owner_proc) {
        disable_fpu_exception();
        save_local_fpu(fpu_owner_proc, TRUE);
        get_cpulocal_var(fpu_owner) = NULL;
    }
    disable_fpu_exception();
    fpu_to_image(image, &vcpu->fpu);
    __asm__ __volatile__("fxrstor64 %0" ::"m"(image) : "memory");
    svm_enter_guest(vcpu);
    disable_fpu_exception();
    __asm__ __volatile__("fxsave64 %0" : "=m"(image)::"memory");
    image_to_fpu(&vcpu->fpu, image);
    enable_fpu_exception();
    vcpu->gprs[VMX_GPR_RAX] = vmcb->save.rax;
    vcpu->running = 0;
    svm_current_vcpu = NULL;
    stop_context(p);
    fill_exit(vcpu, exit_out);
    if (vmcb->control.exit_code == ~0ULL) {
        printk("svm: invalid VMCB: cr0=%lx cr3=%lx cr4=%lx efer=%lx "
               "rip=%lx rflags=%lx ncr3=%lx asid=%u intctl=%x "
               "iopm=%lx msrpm=%lx intercept=%lx\n",
               (unsigned long)vmcb->save.cr0, (unsigned long)vmcb->save.cr3,
               (unsigned long)vmcb->save.cr4, (unsigned long)vmcb->save.efer,
               (unsigned long)vmcb->save.rip, (unsigned long)vmcb->save.rflags,
               (unsigned long)vmcb->control.n_cr3, vmcb->control.asid,
               vmcb->control.int_ctl, (unsigned long)vmcb->control.iopm_base_pa,
               (unsigned long)vmcb->control.msrpm_base_pa,
               (unsigned long)vmcb->control.intercept);
    }
    return 0;
}

static void svm_proc_cleanup(struct proc* p)
{
    int i;
    for (i = 0; i < SVM_MAX_VMS; i++)
        if (vm_table[i].in_use && vm_table[i].owner == p->endpoint)
            svm_vm_destroy(p->endpoint, HANDLE(vm_table[i].gen, i));
    for (i = 0; i < SVM_MAX_VCPUS; i++)
        if (vcpu_table[i].in_use && vcpu_table[i].owner == p->endpoint &&
            !vcpu_table[i].vm->in_use)
            destroy_vcpu(&vcpu_table[i]);
}

static const struct hv_backend_ops svm_backend_ops = {
    .backend = HV_BACKEND_SVM,
    .state_size = sizeof(struct vmx_guest_state),
    .exit_size = sizeof(struct vmx_exit),
    .query_caps = svm_query_caps,
    .vm_create = svm_vm_create,
    .vm_destroy = svm_vm_destroy,
    .vm_set_memslot = svm_vm_set_memslot,
    .vcpu_create = svm_vcpu_create,
    .vcpu_destroy = svm_vcpu_destroy,
    .vcpu_set_state = svm_set_state,
    .vcpu_get_state = svm_get_state,
    .vcpu_run = svm_run,
    .proc_cleanup = svm_proc_cleanup,
};
