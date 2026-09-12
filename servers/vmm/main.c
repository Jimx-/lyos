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
 * vmm: userspace virtual machine monitor.
 *
 * Owns the VMs created through the kernel hypervisor interface, allocates
 * guest RAM through MM, and runs guests under the fixed first-version
 * policy implemented in run.c. Clients manage their VMs over IPC
 * (VMM_* messages, see include/uapi/lyos/vmm.h).
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/endpoint.h>
#include <lyos/hypervisor.h>
#include <lyos/vmm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <errno.h>
#include <string.h>

#include "proto.h"
#include "const.h"
#include "type.h"
#include "arch.h"

struct vmm_vm vmm_vm_table[VMM_NR_VMS];

int vmm_available;
struct hv_caps vmm_hw_caps;

static void vmm_init(void);
static clock_t reap_interval;

static void arm_reaper(void)
{
    if (vmm_available && kernel_alarm(reap_interval, 0) != 0)
        panic("vmm: cannot arm client cleanup alarm");
}

int main()
{
    vmm_init();

    while (TRUE) {
        MESSAGE msg;

        if (send_recv(RECEIVE, ANY, &msg) != 0) continue;
        int src = msg.source;

        if (msg.type == NOTIFY_MSG) {
            if (src == CLOCK) {
                vm_reap_dead_clients();
                arm_reaper();
            }
            continue;
        }

        vm_reap_dead_clients();

        switch (msg.type) {
        case VMM_QUERY:
            msg.u.m_vmm.retval = do_vmm_query(&msg);
            break;
        case VMM_VM_CREATE:
            msg.u.m_vmm.retval = do_vmm_vm_create(&msg);
            break;
        case VMM_VM_DESTROY:
            msg.u.m_vmm.retval = do_vmm_vm_destroy(&msg);
            break;
        case VMM_VM_WRITE:
            msg.u.m_vmm.retval = do_vmm_vm_write(&msg);
            break;
        case VMM_VM_READ:
            msg.u.m_vmm.retval = do_vmm_vm_read(&msg);
            break;
        case VMM_VM_RUN:
            msg.u.m_vmm.retval = do_vmm_vm_run(&msg);
            break;
        default:
            msg.u.m_vmm.retval = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
        vm_reap_dead_clients();
    }

    return 0;
}

static void vmm_init()
{
    int retval;

    /* complete service up before serving any request */
    if ((retval = serv_init()) != 0) {
        printl("VMM: service init failed: %d\n", retval);
        return;
    }

    memset(&vmm_hw_caps, 0, sizeof(vmm_hw_caps));
    retval = hv_query_caps(&vmm_hw_caps);
    if (retval == 0 && (vmm_hw_caps.caps & HV_CAP_ACTIVE) &&
        (vmm_hw_caps.caps & HV_CAP_STAGE2) &&
        vmm_hw_caps.backend == vmm_arch_ops.backend) {
        reap_interval = get_system_hz();
        if (reap_interval <= 0) panic("vmm: invalid system clock frequency");
        vmm_available = 1;
        arm_reaper();
        printl("VMM: virtual machine monitor is running (virtualization "
               "active).\n");
    } else {
        vmm_available = 0;
        printl("VMM: virtual machine monitor is running (virtualization "
               "unavailable, "
               "query retval %d).\n",
               retval);
    }
}
