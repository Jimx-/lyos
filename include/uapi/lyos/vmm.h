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

#ifndef _UAPI_LYOS_VMM_H_
#define _UAPI_LYOS_VMM_H_

#include <lyos/types.h>
#include <lyos/hypervisor.h>

/*
 * Userspace virtual machine monitor (servers/vmm) IPC protocol.
 *
 * The server owns VMs through NR_HVCTL and MM guest RAM objects. Guest
 * setup and exit emulation are supplied by the selected architecture backend.
 */

#define VMM_ABI_VERSION 2

/* guest RAM is mapped page-wise; sizes are rounded to this granularity */
#define VMM_PAGE_SIZE 4096UL

/* per-VM and total guest RAM limits, matching MM's guest RAM objects */
#define VMM_MEM_MAX   (16UL << 20)
#define VMM_MEM_TOTAL (64UL << 20)

/* run status codes (struct vmm_run_status.status) */
#define VMM_RUN_HALTED                                                        \
    0                     /* guest halted; pc identifies the next instruction \
                           */
#define VMM_RUN_FAULTED 1 /* guest exit the policy cannot continue from */
#define VMM_RUN_ERROR   2 /* kernel or server side error */

/* Normalized run outcomes, independent of hardware exit encodings. */
#define VMM_EXIT_NONE            0
#define VMM_EXIT_HALT            1
#define VMM_EXIT_MEMORY_FAULT    2
#define VMM_EXIT_GUEST_EXCEPTION 3
#define VMM_EXIT_UNSUPPORTED     4
#define VMM_EXIT_ENTRY_FAILURE   5
#define VMM_EXIT_INTERNAL_ERROR  6
#define VMM_EXIT_RUN_LIMIT       7
#define VMM_EXIT_INTERRUPTED     8 /* host event prevented continuation */

struct vmm_caps {
    __u32 abi_version;
    __u32 caps; /* HV_CAP_* bitmask, 0 if virtualization is unavailable */
    __u32 nr_vms;
    __u32 backend; /* HV_BACKEND_*; implementation providing virtualization */
};

struct vmm_run_status {
    __u32 status;      /* VMM_RUN_* */
    __u32 exit_reason; /* VMM_EXIT_* */
    __u64 pc;          /* guest instruction address associated with the outcome;
                        * next instruction after a halt, zero when unavailable */
};

/* lib/liblyos/vmm.c */
int vmm_query(struct vmm_caps* caps);
int vmm_vm_create(unsigned long mem_size, unsigned long entry, u32* id);
int vmm_vm_destroy(u32 id);
int vmm_vm_write(u32 id, u64 gpa, const void* buf, size_t len);
int vmm_vm_read(u32 id, u64 gpa, void* buf, size_t len);
int vmm_vm_run(u32 id, struct vmm_run_status* status);

#endif /* _UAPI_LYOS_VMM_H_ */
