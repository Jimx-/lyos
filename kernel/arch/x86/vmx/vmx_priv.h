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

#ifndef _VMX_PRIV_H_
#define _VMX_PRIV_H_

#include "vmx.h"
#include <asm/protect.h>
#include <asm/smp.h>

struct tss;
extern struct tss tss[];

struct vmx_state_info {
    int vmxon_active;
    int caps;
    u32 vmcs_revision;
    u32 mxcsr_mask;
    u64 msr_basic;
    u64 msr_pinbased, msr_procbased, msr_procbased2, msr_exit, msr_entry;
    u64 msr_cr0_fixed0, msr_cr0_fixed1, msr_cr4_fixed0, msr_cr4_fixed1;
    u64 ept_vpid_cap;
    u32 eptp_encoding; /* verified VMCS encoding of the EPT pointer */
    u64 exit_rsp;      /* top of the dedicated VM exit stack */
    u64 current_vmcs;  /* physical address of the current VMCS, 0 = none */
};

extern struct vmx_state_info vmx_state;
extern struct vmx_vcpu* vmx_current_vcpu; /* asm scratch, valid during a run */
extern void vmx_exit_entry(void);         /* assembly VM exit stub (HOST_RIP) */
extern char vmx_exit_stack_top[];         /* defined in vmenter.S */

u32 vmx_adjust_control(u32 desired, u64 msr_value);
int vmx_vmcs_load(u64 vmcs_phys);
int vmx_vmcs_clear(u64 vmcs_phys);
int vmx_vmwrite(u32 field, u64 value);
u64 vmx_vmread(u32 field);
int vmx_vmcs_setup(struct vmx_vcpu* vcpu);
int vmx_validate_state(const struct vmx_guest_state* st);
int vmx_vmcs_write_state(struct vmx_vcpu* vcpu,
                         const struct vmx_guest_state* st);
void vmx_vmcs_read_state(struct vmx_vcpu* vcpu, struct vmx_guest_state* st);
int vmx_vmcs_refresh_host(struct vmx_vcpu* vcpu);

/* ept.c */
int vmx_ept_init_vm(struct vmx_vm* vm);
void vmx_ept_destroy_vm(struct vmx_vm* vm);
int vmx_ept_map_page(struct vmx_vm* vm, u64 gpa, u64 pa, u32 flags);
void vmx_ept_invalidate(struct vmx_vm* vm);
u32 vmx_ept_pages_in_use(void);
int vmx_ept_supported(void);

#endif /* _VMX_PRIV_H_ */
