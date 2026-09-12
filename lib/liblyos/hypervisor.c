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
#include <string.h>
#include <errno.h>
#include "lyos/const.h"
#include <lyos/hypervisor.h>
#include <lyos/sysutils.h>

static int hv_call(MESSAGE* m)
{
    int retval = syscall_entry(NR_HVCTL, m);
    if (retval == -1 && errno == ENOSYS) return ENOSYS;
    return retval;
}

int hv_query_caps(struct hv_caps* caps)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_QUERY_CAPS;
    m.u.m_hvctl.buf = caps;
    m.u.m_hvctl.buf_len = sizeof(*caps);

    return hv_call(&m);
}

int hv_vm_create(u32* handle)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.u.m_hvctl.request = HVCTL_VM_CREATE;

    if ((retval = hv_call(&m)) != 0) return retval;
    *handle = m.u.m_hvctl.vm_handle;
    return 0;
}

int hv_vm_destroy(u32 handle)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VM_DESTROY;
    m.u.m_hvctl.vm_handle = handle;

    return hv_call(&m);
}

int hv_vm_set_memslot(u32 vm, const struct hv_memslot* slot)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VM_SET_MEMSLOT;
    m.u.m_hvctl.vm_handle = vm;
    m.u.m_hvctl.buf = (void*)slot;
    m.u.m_hvctl.buf_len = sizeof(*slot);

    return hv_call(&m);
}

int hv_vcpu_create(u32 vm, u32* vcpu)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.u.m_hvctl.request = HVCTL_VCPU_CREATE;
    m.u.m_hvctl.vm_handle = vm;

    if ((retval = hv_call(&m)) != 0) return retval;
    *vcpu = m.u.m_hvctl.vcpu_handle;
    return 0;
}

int hv_vcpu_destroy(u32 vcpu)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VCPU_DESTROY;
    m.u.m_hvctl.vcpu_handle = vcpu;

    return hv_call(&m);
}

int hv_vcpu_set_state(u32 vcpu, const void* state, size_t size)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VCPU_SET_STATE;
    m.u.m_hvctl.vcpu_handle = vcpu;
    m.u.m_hvctl.buf = (void*)state;
    m.u.m_hvctl.buf_len = size;

    return hv_call(&m);
}

int hv_vcpu_get_state(u32 vcpu, void* state, size_t size)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VCPU_GET_STATE;
    m.u.m_hvctl.vcpu_handle = vcpu;
    m.u.m_hvctl.buf = (void*)state;
    m.u.m_hvctl.buf_len = size;

    return hv_call(&m);
}

int hv_vcpu_run(u32 vcpu, void* exit, size_t size)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_VCPU_RUN;
    m.u.m_hvctl.vcpu_handle = vcpu;
    m.u.m_hvctl.buf = exit;
    m.u.m_hvctl.buf_len = size;

    return hv_call(&m);
}

int hv_check_client(endpoint_t client)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_CHECK_CLIENT;
    m.u.m_hvctl.value = client;
    return hv_call(&m);
}

/* MM-only operations */

int hv_register_backing(u32 mm_handle, unsigned long npages, u64* pages)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_REGISTER_BACKING;
    m.u.m_hvctl.gpa = mm_handle;
    m.u.m_hvctl.value = npages;
    m.u.m_hvctl.buf = pages;
    m.u.m_hvctl.buf_len = npages * sizeof(u64);

    return hv_call(&m);
}

int hv_get_release(u32* mm_handle)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.u.m_hvctl.request = HVCTL_GET_RELEASE;

    if ((retval = hv_call(&m)) != 0) return retval;
    *mm_handle = (u32)m.u.m_hvctl.gpa;
    return 0;
}

int hv_unregister_backing(u32 mm_handle)
{
    MESSAGE m;
    memset(&m, 0, sizeof(m));

    m.u.m_hvctl.request = HVCTL_UNREGISTER_BACKING;
    m.u.m_hvctl.gpa = mm_handle;
    return hv_call(&m);
}

/* guest RAM objects (MM) */

int mm_guest_ram_alloc(size_t length, u32* handle, void** vaddr)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = MM_GUEST_RAM_ALLOC;
    m.u.m_mm_guestram.who = SELF;
    m.u.m_mm_guestram.length = length;

    if ((retval = send_recv(BOTH, TASK_MM, &m)) != 0) return retval;
    if (m.u.m_mm_guestram.retval != 0) return m.u.m_mm_guestram.retval;

    *handle = m.u.m_mm_guestram.ret_handle;
    *vaddr = m.u.m_mm_guestram.vaddr;
    return 0;
}

int mm_guest_ram_free(u32 handle)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = MM_GUEST_RAM_FREE;
    m.u.m_mm_guestram.handle = handle;

    if ((retval = send_recv(BOTH, TASK_MM, &m)) != 0) return retval;
    return m.u.m_mm_guestram.retval;
}
