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
 * VM lifecycle and guest memory: the server is the VM owner, so every
 * request is checked against the creating endpoint.
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/endpoint.h>
#include <lyos/hypervisor.h>
#include <lyos/vmm.h>
#include <lyos/sysutils.h>
#include <errno.h>
#include <string.h>

#include "proto.h"
#include "const.h"
#include "type.h"
#include "arch.h"

extern struct vmm_vm vmm_vm_table[VMM_NR_VMS];
extern int vmm_available;
extern struct hv_caps vmm_hw_caps;

static u32 next_vm_id;
static u64 total_mem_size;

static struct vmm_vm* vm_slot_alloc(void)
{
    int i;

    for (i = 0; i < VMM_NR_VMS; i++) {
        if (!vmm_vm_table[i].in_use) return &vmm_vm_table[i];
    }
    return NULL;
}

struct vmm_vm* vm_lookup(u32 id, endpoint_t owner)
{
    int i;

    if (id == 0) return NULL;

    for (i = 0; i < VMM_NR_VMS; i++) {
        struct vmm_vm* vm = &vmm_vm_table[i];

        if (vm->in_use && vm->id == id) {
            if (vm->owner != owner) return NULL;
            return vm;
        }
    }
    return NULL;
}

int do_vmm_query(MESSAGE* m)
{
    struct mess_vmm* req = &m->u.m_vmm;
    struct vmm_caps caps;
    int i;

    memset(&caps, 0, sizeof(caps));
    caps.abi_version = VMM_ABI_VERSION;
    caps.caps = vmm_available ? vmm_hw_caps.caps : 0;
    caps.backend = vmm_hw_caps.backend;

    for (i = 0; i < VMM_NR_VMS; i++) {
        if (vmm_vm_table[i].in_use) caps.nr_vms++;
    }

    if (!req->buf) return EINVAL;
    if (req->buf_len < sizeof(caps)) return EINVAL;

    return data_copy(m->source, req->buf, SELF, &caps, sizeof(caps));
}

int do_vmm_vm_create(MESSAGE* m)
{
    struct mess_vmm* req = &m->u.m_vmm;
    struct hv_memslot slot;
    struct vmm_vm* vm;
    u64 mem_size, entry;
    int retval;

    if (!vmm_available) return ENODEV;

    mem_size = req->mem_size;
    entry = req->value;
    if (mem_size == 0) return EINVAL;
    if (entry >= mem_size) return EINVAL;

    mem_size = (mem_size + VMM_PAGE_SIZE - 1) & ~(u64)(VMM_PAGE_SIZE - 1);
    if (mem_size > VMM_MEM_MAX) return EINVAL;
    if (total_mem_size + mem_size > VMM_MEM_TOTAL) return ENOMEM;

    if ((vm = vm_slot_alloc()) == NULL) return ENOMEM;

    memset(vm, 0, sizeof(*vm));
    vm->mem_size = mem_size;

    if ((retval = mm_guest_ram_alloc((size_t)mem_size, &vm->mm_handle,
                                     &vm->ram)) != 0)
        return retval;

    if ((retval = hv_vm_create(&vm->vm_handle)) != 0) goto free_ram;

    memset(&slot, 0, sizeof(slot));
    slot.gpa = 0;
    slot.len = mem_size;
    slot.offset = 0;
    slot.mm_handle = vm->mm_handle;
    slot.flags = HV_MEM_ALL;
    if ((retval = hv_vm_set_memslot(vm->vm_handle, &slot)) != 0)
        goto destroy_vm;

    if ((retval = hv_vcpu_create(vm->vm_handle, &vm->vcpu_handle)) != 0)
        goto destroy_vm;

    if ((retval = vmm_arch_ops.init_guest(vm, entry)) != 0) goto destroy_vcpu;

    if (++next_vm_id == 0) next_vm_id = 1;
    vm->id = next_vm_id;
    vm->owner = m->source;
    vm->in_use = 1;
    total_mem_size += mem_size;

    req->vm_id = vm->id;
    return 0;

destroy_vcpu:
    hv_vcpu_destroy(vm->vcpu_handle);
destroy_vm:
    hv_vm_destroy(vm->vm_handle);
free_ram:
    mm_guest_ram_free(vm->mm_handle);
    return retval;
}

static int vm_destroy(struct vmm_vm* vm)
{
    int retval;

    /* VM destruction also destroys its vCPU. Record each completed step so
     * cleanup can be retried if dropping the MM mapping fails.
     */
    if (vm->vm_handle) {
        if ((retval = hv_vm_destroy(vm->vm_handle)) != 0) return retval;
        vm->vm_handle = 0;
        vm->vcpu_handle = 0;
    }

    /* the kernel released the backing; drop the VMM mapping */
    if (vm->mm_handle) {
        if ((retval = mm_guest_ram_free(vm->mm_handle)) != 0) return retval;
        vm->mm_handle = 0;
        vm->ram = NULL;
    }

    total_mem_size -= vm->mem_size;
    vm->in_use = 0;
    return 0;
}

int do_vmm_vm_destroy(MESSAGE* m)
{
    struct vmm_vm* vm = vm_lookup(m->u.m_vmm.vm_id, m->source);

    if (!vm) return EINVAL;
    return vm_destroy(vm);
}

void vm_reap_dead_clients(void)
{
    int i, retval;

    for (i = 0; i < VMM_NR_VMS; i++) {
        struct vmm_vm* vm = &vmm_vm_table[i];

        if (!vm->in_use) continue;
        if (hv_check_client(vm->owner) != ESRCH) continue;
        if ((retval = vm_destroy(vm)) != 0)
            printl("vmm: vm%u: dead-client cleanup failed: %d\n", vm->id,
                   retval);
    }
}

static int check_mem_range(struct vmm_vm* vm, u64 gpa, size_t len)
{
    if (len == 0) return EINVAL;
    if (gpa > vm->mem_size) return EINVAL;
    if (len > vm->mem_size - gpa) return EINVAL;
    return 0;
}

int do_vmm_vm_write(MESSAGE* m)
{
    struct mess_vmm* req = &m->u.m_vmm;
    struct vmm_vm* vm = vm_lookup(req->vm_id, m->source);
    int retval;

    if (!vm) return EINVAL;
    if (!req->buf) return EINVAL;
    if ((retval = check_mem_range(vm, req->gpa, req->buf_len)) != 0)
        return retval;

    return data_copy(SELF, (char*)vm->ram + (unsigned long)req->gpa, m->source,
                     req->buf, req->buf_len);
}

int do_vmm_vm_read(MESSAGE* m)
{
    struct mess_vmm* req = &m->u.m_vmm;
    struct vmm_vm* vm = vm_lookup(req->vm_id, m->source);
    int retval;

    if (!vm) return EINVAL;
    if (!req->buf) return EINVAL;
    if ((retval = check_mem_range(vm, req->gpa, req->buf_len)) != 0)
        return retval;

    return data_copy(m->source, req->buf, SELF,
                     (char*)vm->ram + (unsigned long)req->gpa, req->buf_len);
}
