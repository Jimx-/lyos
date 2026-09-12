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
#include <lyos/const.h>
#include <lyos/endpoint.h>
#include <errno.h>
#include <string.h>
#include <kernel/proc.h>
#include <kernel/proto.h>
#include <kernel/hypervisor.h>

/* System calls are serialized by the kernel. Backend record sizes are checked
 * at registration, before any user copy can use these staging buffers.
 */
static struct hv_caps caps_buf;
static struct hv_memslot memslot_buf;
static u8 state_buf[HV_STATE_MAX_SIZE] __attribute__((aligned(16)));
static u8 exit_buf[HV_EXIT_MAX_SIZE] __attribute__((aligned(16)));
static u64 backing_list[HV_BACKING_POOL_PAGES];

int sys_hvctl(MESSAGE* m, struct proc* p)
{
    int request = m->u.m_hvctl.request;
    int retval;
    u32 handle;

    if (!hv_backend && request != HVCTL_QUERY_CAPS &&
        request != HVCTL_GET_RELEASE && request != HVCTL_CHECK_CLIENT)
        return EOPNOTSUPP;

    switch (request) {
    case HVCTL_CHECK_CLIENT: {
        endpoint_t client = m->u.m_hvctl.value;
        int slot = ENDPOINT_P(m->u.m_hvctl.value);
        struct proc* target;

        if (client < 0 || (u64)client != m->u.m_hvctl.value || slot < 0 ||
            slot >= NR_PROCS)
            return ESRCH;
        target = proc_addr(slot);
        if (target->endpoint != client || PST_IS_SET(target, PST_FREE_SLOT))
            return ESRCH;
        return 0;
    }

    case HVCTL_QUERY_CAPS:
        memset(&caps_buf, 0, sizeof(caps_buf));
        if (hv_backend) {
            if ((retval = hv_backend->query_caps(&caps_buf)) != 0)
                return retval;
            caps_buf.backend = hv_backend->backend;
        }
        caps_buf.abi_version = HV_ABI_VERSION;
        hv_backing_query_caps(&caps_buf);

        m->u.m_hvctl.value = caps_buf.caps;
        m->u.m_hvctl.gpa = caps_buf.abi_version;
        if (m->u.m_hvctl.buf) {
            if (m->u.m_hvctl.buf_len < sizeof(caps_buf)) return EINVAL;
            if ((retval =
                     data_vir_copy_check(p, p->endpoint, m->u.m_hvctl.buf,
                                         KERNEL, &caps_buf, sizeof(caps_buf))))
                return retval;
        }
        return 0;

    case HVCTL_VM_CREATE:
        retval = hv_backend->vm_create(p->endpoint, &handle);
        if (!retval) m->u.m_hvctl.vm_handle = handle;
        return retval;

    case HVCTL_VM_DESTROY:
        return hv_backend->vm_destroy(p->endpoint, m->u.m_hvctl.vm_handle);

    case HVCTL_VM_SET_MEMSLOT:
        if (m->u.m_hvctl.buf_len != sizeof(memslot_buf)) return EINVAL;
        if ((retval =
                 data_vir_copy_check(p, KERNEL, &memslot_buf, p->endpoint,
                                     m->u.m_hvctl.buf, sizeof(memslot_buf))))
            return retval;
        return hv_backend->vm_set_memslot(p->endpoint, m->u.m_hvctl.vm_handle,
                                          &memslot_buf);

    case HVCTL_VCPU_CREATE:
        retval = hv_backend->vcpu_create(p->endpoint, m->u.m_hvctl.vm_handle,
                                         &handle);
        if (!retval) m->u.m_hvctl.vcpu_handle = handle;
        return retval;

    case HVCTL_VCPU_DESTROY:
        return hv_backend->vcpu_destroy(p->endpoint, m->u.m_hvctl.vcpu_handle);

    case HVCTL_VCPU_SET_STATE:
        if (m->u.m_hvctl.buf_len != hv_backend->state_size) return EINVAL;
        if ((retval =
                 data_vir_copy_check(p, KERNEL, state_buf, p->endpoint,
                                     m->u.m_hvctl.buf, hv_backend->state_size)))
            return retval;
        return hv_backend->vcpu_set_state(p->endpoint, m->u.m_hvctl.vcpu_handle,
                                          state_buf);

    case HVCTL_VCPU_GET_STATE:
        if (m->u.m_hvctl.buf_len < hv_backend->state_size) return EINVAL;
        if ((retval = hv_backend->vcpu_get_state(
                 p->endpoint, m->u.m_hvctl.vcpu_handle, state_buf)) != 0)
            return retval;
        return data_vir_copy_check(p, p->endpoint, m->u.m_hvctl.buf, KERNEL,
                                   state_buf, hv_backend->state_size);

    case HVCTL_VCPU_RUN:
        if (m->u.m_hvctl.buf_len < hv_backend->exit_size) return EINVAL;
        if ((retval = hv_backend->vcpu_run(
                 p->endpoint, m->u.m_hvctl.vcpu_handle, exit_buf)) != 0)
            return retval;
        /*
         * Plain (non-resumable) copy: replaying VCPU_RUN after a page fault
         * would re-enter the guest.
         */
        return data_vir_copy(p->endpoint, m->u.m_hvctl.buf, KERNEL, exit_buf,
                             hv_backend->exit_size);

    case HVCTL_REGISTER_BACKING:
        if (p->endpoint != TASK_MM) return EPERM;
        if (m->u.m_hvctl.value == 0 ||
            m->u.m_hvctl.value > HV_BACKING_POOL_PAGES)
            return EINVAL;
        if (m->u.m_hvctl.buf_len != (size_t)m->u.m_hvctl.value * sizeof(u64))
            return EINVAL;
        if ((retval = data_vir_copy_check(
                 p, KERNEL, backing_list, p->endpoint, m->u.m_hvctl.buf,
                 (size_t)m->u.m_hvctl.value * sizeof(u64))))
            return retval;
        return hv_backing_register((u32)m->u.m_hvctl.gpa,
                                   (unsigned long)m->u.m_hvctl.value,
                                   backing_list);

    case HVCTL_GET_RELEASE:
        if (p->endpoint != TASK_MM) return EPERM;
        retval = hv_backing_release_get(&handle);
        if (!retval) m->u.m_hvctl.gpa = handle;
        return retval;

    case HVCTL_UNREGISTER_BACKING:
        if (p->endpoint != TASK_MM) return EPERM;
        return hv_backing_unregister((u32)m->u.m_hvctl.gpa);

    default:
        return EINVAL;
    }
}
