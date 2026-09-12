/*
    (c)Copyright 2026 Jimx

    This file is part of Lyos.

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

/*
 * Guest execution policy: runs the bound vCPU and services the exits
 * the first version can continue from. Host external interrupts are
 * re-entered transparently, CPUID is emulated with a constrained
 * x87/SSE model, port I/O is emulated (0xe9 is the debug console),
 * and HLT ends a run with the rip advanced past the hlt. Every other
 * exit stops the run with a normalized result; raw details stay in the log.
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/endpoint.h>
#include <asm/vmx.h>
#include <lyos/vmm.h>
#include <lyos/sysutils.h>
#include <errno.h>
#include <string.h>

#include "proto.h"
#include "const.h"
#include "type.h"
#include "arch.h"

#define VMM_GDTR_BASE          0x3000
#define VMM_GDTR_LIMIT         0x1f
#define VMM_DEBUG_PORT         0xe9
#define VMM_RUN_MAX_INTERRUPTS 10000

/* CPUID leaf 1 EDX feature bits offered to guests */
#define CPUID1_FPU  (1U << 0)
#define CPUID1_TSC  (1U << 4)
#define CPUID1_MSR  (1U << 5)
#define CPUID1_CX8  (1U << 8)
#define CPUID1_CMOV (1U << 15)
#define CPUID1_MMX  (1U << 23)
#define CPUID1_FXSR (1U << 24)
#define CPUID1_SSE  (1U << 25)
#define CPUID1_SSE2 (1U << 26)

#define CPUID1_EDX                                                     \
    (CPUID1_FPU | CPUID1_TSC | CPUID1_MSR | CPUID1_CX8 | CPUID1_CMOV | \
     CPUID1_MMX | CPUID1_FXSR | CPUID1_SSE | CPUID1_SSE2)

#define VMM_CPUID_MAX_BASIC    1
#define VMM_CPUID_MAX_EXTENDED 0x80000001U

#define MSR_EFER 0xc0000080

/* diagnostics per run for accesses the policy bus cannot serve */
#define VMM_IO_LOG_MAX 8

static void fill_fault(struct vmm_run_status* status,
                       const struct vmx_exit* exit)
{
    status->status = VMM_RUN_FAULTED;
    status->pc = exit->rip;
    if (exit->entry_failure) {
        status->status = VMM_RUN_ERROR;
        status->exit_reason = VMM_EXIT_ENTRY_FAILURE;
        return;
    }

    switch (exit->reason) {
    case VMX_EXIT_EPT_VIOLATION:
        status->exit_reason = VMM_EXIT_MEMORY_FAULT;
        break;
    case VMX_EXIT_EXCEPTION:
        /* VMX shares this reason between guest exceptions and host NMIs. */
        if ((exit->intr_info & (7U << 8)) == (2U << 8)) {
            status->status = VMM_RUN_ERROR;
            status->exit_reason = VMM_EXIT_INTERRUPTED;
            break;
        }
        status->exit_reason = VMM_EXIT_GUEST_EXCEPTION;
        break;
    case VMX_EXIT_TRIPLE_FAULT:
        status->exit_reason = VMM_EXIT_GUEST_EXCEPTION;
        break;
    case VMX_EXIT_EPT_MISCONFIGURATION:
        status->status = VMM_RUN_ERROR;
        status->exit_reason = VMM_EXIT_INTERNAL_ERROR;
        break;
    default:
        status->exit_reason = VMM_EXIT_UNSUPPORTED;
        break;
    }
}

static void fill_emulation_error(struct vmm_run_status* status, int retval)
{
    status->status = retval == EINVAL ? VMM_RUN_FAULTED : VMM_RUN_ERROR;
    status->exit_reason =
        retval == EINVAL ? VMM_EXIT_UNSUPPORTED : VMM_EXIT_INTERNAL_ERROR;
}

/* advance rip within an already fetched state and push it back */
static int advance_rip(struct vmm_vm* vm, const struct vmx_exit* exit,
                       struct vmx_guest_state* st)
{
    if (!(exit->flags & VMX_EXIT_F_INSN_LEN)) return EINVAL;

    st->rip = exit->rip + exit->insn_len;
    return hv_vcpu_set_state(vm->vcpu_handle, st, sizeof(*st));
}

/* fetch the current state, then advance rip past the exit instruction */
static int resume_past(struct vmm_vm* vm, const struct vmx_exit* exit)
{
    struct vmx_guest_state st;

    if (hv_vcpu_get_state(vm->vcpu_handle, &st, sizeof(st)) != 0) return EIO;
    return advance_rip(vm, exit, &st);
}

static void cpuid_leaf(u32 leaf, u32* ax, u32* bx, u32* cx, u32* dx)
{
    *ax = *bx = *cx = *dx = 0;

    switch (leaf) {
    case 0:
        *ax = VMM_CPUID_MAX_BASIC;
        *bx = 0x736f794c; /* "Lyos" */
        *dx = 0x004d4d56; /* "VMM" */
        break;
    case 1:
        *ax = 0x00000600; /* family 6, model 0, stepping 0 */
        *dx = CPUID1_EDX;
        break;
    default:
        if (leaf == 0x80000000) *ax = VMM_CPUID_MAX_EXTENDED;
        /* extended leaves and brand strings: zeros, no long mode */
        break;
    }
}

static int emulate_cpuid(struct vmm_vm* vm, const struct vmx_exit* exit)
{
    struct vmx_guest_state st;
    u32 ax, bx, cx, dx;

    if (hv_vcpu_get_state(vm->vcpu_handle, &st, sizeof(st)) != 0) return EIO;

    cpuid_leaf((u32)st.gprs[VMX_GPR_RAX], &ax, &bx, &cx, &dx);
    st.gprs[VMX_GPR_RAX] = ax;
    st.gprs[VMX_GPR_RBX] = bx;
    st.gprs[VMX_GPR_RCX] = cx;
    st.gprs[VMX_GPR_RDX] = dx;

    if (advance_rip(vm, exit, &st) != 0) return EIO;
    return 0;
}

static int emulate_io(struct vmm_vm* vm, const struct vmx_exit* exit,
                      unsigned long* logged)
{
    struct vmx_guest_state st;
    unsigned long qual = (unsigned long)exit->qualification;
    u32 port = VMX_IO_PORT(qual);
    unsigned width = VMX_IO_WIDTH(qual);
    int is_in = VMX_IO_IS_IN(qual);
    u32 value;

    if (VMX_IO_IS_STRING(qual)) return EINVAL; /* no string I/O support */
    if (width != 1 && width != 2 && width != 4) return EINVAL;

    if (hv_vcpu_get_state(vm->vcpu_handle, &st, sizeof(st)) != 0) return EIO;
    value = (u32)st.gprs[VMX_GPR_RAX];

    if (is_in) {
        /* nothing is attached behind the policy bus */
        u32 mask = width == 4 ? 0xffffffffU : (1U << (width * 8)) - 1;
        st.gprs[VMX_GPR_RAX] = (value & ~mask) | mask;
        if (*logged < VMM_IO_LOG_MAX) {
            (*logged)++;
            printl("vmm: vm%u: unhandled in port %#x width %u\n", vm->id, port,
                   width);
        }
    } else if (port == VMM_DEBUG_PORT) {
        if (width == 1) {
            printl("%c", (unsigned char)value);
        } else {
            printl("vmm: vm%u: debug port write %#x\n", vm->id, value);
        }
    } else {
        if (*logged < VMM_IO_LOG_MAX) {
            (*logged)++;
            printl("vmm: vm%u: unhandled out port %#x width %u value %#x\n",
                   vm->id, port, width, value);
        }
    }

    if (advance_rip(vm, exit, &st) != 0) return EIO;
    return 0;
}

static int emulate_rdmsr(struct vmm_vm* vm, const struct vmx_exit* exit)
{
    struct vmx_guest_state st;

    if (hv_vcpu_get_state(vm->vcpu_handle, &st, sizeof(st)) != 0) return EIO;

    if ((u32)st.gprs[VMX_GPR_RCX] != MSR_EFER) return EINVAL;

    st.gprs[VMX_GPR_RAX] = 0;
    st.gprs[VMX_GPR_RDX] = 0;

    if (advance_rip(vm, exit, &st) != 0) return EIO;
    return 0;
}

static int run_guest(struct vmm_vm* vm, struct vmm_run_status* status)
{
    struct vmx_exit exit;
    unsigned long logged = 0;
    int i, retval, have_exit = 0;

    memset(status, 0, sizeof(*status));

    for (i = 0; i < VMM_RUN_MAX_INTERRUPTS; i++) {
        have_exit = 0;
        retval = hv_vcpu_run(vm->vcpu_handle, &exit, sizeof(exit));
        if (retval != 0) {
            status->status = VMM_RUN_ERROR;
            status->exit_reason = VMM_EXIT_INTERNAL_ERROR;
            status->pc = 0;
            printl("vmm: vm%u: run failed: %d\n", vm->id, retval);
            goto out;
        }
        have_exit = 1;
        status->pc = exit.rip;

        if (exit.entry_failure) {
            fill_fault(status, &exit);
            printl("vmm: vm%u: vm-entry failure (reason %u)\n", vm->id,
                   exit.reason);
            goto out;
        }

        switch (exit.reason) {
        case VMX_EXIT_EXTERNAL_INTERRUPT:
            /* serviced by the host; re-enter without advancing rip */
            continue;
        case VMX_EXIT_HLT:
            if ((retval = resume_past(vm, &exit)) != 0) {
                status->status = VMM_RUN_ERROR;
                status->exit_reason = VMM_EXIT_INTERNAL_ERROR;
                goto out;
            }
            status->status = VMM_RUN_HALTED;
            status->exit_reason = VMM_EXIT_HALT;
            status->pc = exit.rip + exit.insn_len;
            goto out;
        case VMX_EXIT_CPUID:
            if ((retval = emulate_cpuid(vm, &exit)) != 0) {
                fill_emulation_error(status, retval);
                goto out;
            }
            continue;
        case VMX_EXIT_IO_INSTRUCTION:
            if ((retval = emulate_io(vm, &exit, &logged)) != 0) {
                fill_emulation_error(status, retval);
                printl("vmm: vm%u: unsupported io (qual %#lx)\n", vm->id,
                       (unsigned long)exit.qualification);
                goto out;
            }
            continue;
        case VMX_EXIT_RDMSR:
            if ((retval = emulate_rdmsr(vm, &exit)) != 0) {
                fill_emulation_error(status, retval);
                printl("vmm: vm%u: rejected rdmsr\n", vm->id);
                goto out;
            }
            continue;
        case VMX_EXIT_PAUSE:
        case VMX_EXIT_WBINVD:
            /* benign hints: continue past the instruction */
            if (resume_past(vm, &exit) != 0) {
                status->status = VMM_RUN_ERROR;
                status->exit_reason = VMM_EXIT_INTERNAL_ERROR;
                goto out;
            }
            continue;
        default:
            fill_fault(status, &exit);
            goto out;
        }
    }

    /* Bound both host interruptions and successfully emulated instructions. */
    status->status = VMM_RUN_ERROR;
    status->exit_reason = VMM_EXIT_RUN_LIMIT;

out:
    if (have_exit && status->status != VMM_RUN_HALTED)
        printl("vmm: vm%u: vmx exit %u rip %#llx rflags %#llx intr %#x "
               "gpa %#llx qual %#llx\n",
               vm->id, exit.reason, exit.rip, exit.rflags, exit.intr_info,
               (exit.flags & VMX_EXIT_F_GPA) ? exit.gpa : 0,
               exit.qualification);
    return 0;
}

static void fill_baseline_state(struct vmx_guest_state* st, u64 entry,
                                u64 mem_size)
{
#define FLAT_SEG(seg, sel, type, db)                              \
    do {                                                          \
        (seg).selector = (sel);                                   \
        (seg).base = 0;                                           \
        (seg).limit = 0xffffffff;                                 \
        (seg).ar = (type) | VMX_SEG_S | VMX_SEG_P |               \
                   ((db) ? (VMX_SEG_DB | VMX_SEG_G) : VMX_SEG_G); \
    } while (0)

    memset(st, 0, sizeof(*st));

    st->version = VMX_STATE_VERSION;
    st->size = sizeof(*st);

    FLAT_SEG(st->cs, 0x08, 0xb, 1);
    FLAT_SEG(st->ss, 0x10, 0x3, 1);
    FLAT_SEG(st->ds, 0x10, 0x3, 1);
    FLAT_SEG(st->es, 0x10, 0x3, 1);
    FLAT_SEG(st->fs, 0x10, 0x3, 1);
    FLAT_SEG(st->gs, 0x10, 0x3, 1);

    st->gdtr.base = VMM_GDTR_BASE;
    st->gdtr.limit = VMM_GDTR_LIMIT;
    st->idtr.base = 0;
    st->idtr.limit = 0;

    st->cr0 = 0x00010033; /* PE|MP|ET|NE|WP, paging off */
    st->cr4 = 0x600;      /* OSFXSR|OSXMMEXCPT */
    st->efer = 0;
    st->dr7 = 0x400;
    st->rflags = 0x2; /* interrupts disabled */
    st->rip = entry;
    st->gprs[VMX_GPR_RSP] = mem_size - 16; /* unused by the payloads */

    st->fpu.cwd = 0x37f;
    st->fpu.mxcsr = 0x1f80;
}

static int init_guest(struct vmm_vm* vm, u64 entry)
{
    struct vmx_guest_state state;
    fill_baseline_state(&state, entry, vm->mem_size);
    return hv_vcpu_set_state(vm->vcpu_handle, &state, sizeof(state));
}

const struct vmm_arch_ops vmm_arch_ops = {
    .backend = HV_BACKEND_VMX,
    .init_guest = init_guest,
    .run = run_guest,
};
