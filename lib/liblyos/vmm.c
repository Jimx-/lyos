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

/*
 * Client stubs for the vmm server (servers/vmm). The endpoint SERVMAN
 * published for the service is read from the sysfs mount, so any
 * process - service or plain user program - can use these stubs.
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/vmm.h>
#include <lyos/sysutils.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#define VMM_ENDPOINT_PATH "/sys/services/vmm/endpoint"

endpoint_t __vmm_endpoint = NO_TASK;

static int vmm_resolve_endpoint(void)
{
    char buf[32];
    long v;
    int fd, len;

    fd = open(VMM_ENDPOINT_PATH, O_RDONLY);
    if (fd < 0) return ENOENT;

    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return EIO;

    buf[len] = '\0';
    v = strtol(buf, NULL, 10);
    if (v == LONG_MIN || v == LONG_MAX) return ERANGE;

    __vmm_endpoint = (endpoint_t)v;
    return 0;
}

static int vmm_sendrec(MESSAGE* msg)
{
    int retval;

    if (__vmm_endpoint == NO_TASK) {
        if ((retval = vmm_resolve_endpoint()) != 0) return retval;
    }

    return send_recv(BOTH, __vmm_endpoint, msg);
}

int vmm_query(struct vmm_caps* caps)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_QUERY;
    m.u.m_vmm.buf = caps;
    m.u.m_vmm.buf_len = sizeof(*caps);

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    if (m.u.m_vmm.retval != 0) return m.u.m_vmm.retval;
    if (caps->abi_version != VMM_ABI_VERSION) return EPROTONOSUPPORT;
    return 0;
}

int vmm_vm_create(unsigned long mem_size, unsigned long entry, u32* id)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_VM_CREATE;
    m.u.m_vmm.mem_size = mem_size;
    m.u.m_vmm.value = entry;

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    if ((retval = m.u.m_vmm.retval) != 0) return retval;

    *id = (u32)m.u.m_vmm.vm_id;
    return 0;
}

int vmm_vm_destroy(u32 id)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_VM_DESTROY;
    m.u.m_vmm.vm_id = id;

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    return m.u.m_vmm.retval;
}

int vmm_vm_write(u32 id, u64 gpa, const void* buf, size_t len)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_VM_WRITE;
    m.u.m_vmm.vm_id = id;
    m.u.m_vmm.gpa = gpa;
    m.u.m_vmm.buf = (void*)buf;
    m.u.m_vmm.buf_len = len;

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    return m.u.m_vmm.retval;
}

int vmm_vm_read(u32 id, u64 gpa, void* buf, size_t len)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_VM_READ;
    m.u.m_vmm.vm_id = id;
    m.u.m_vmm.gpa = gpa;
    m.u.m_vmm.buf = buf;
    m.u.m_vmm.buf_len = len;

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    return m.u.m_vmm.retval;
}

int vmm_vm_run(u32 id, struct vmm_run_status* status)
{
    MESSAGE m;
    int retval;

    memset(&m, 0, sizeof(m));
    m.type = VMM_VM_RUN;
    m.u.m_vmm.vm_id = id;
    m.u.m_vmm.buf = status;
    m.u.m_vmm.buf_len = sizeof(*status);

    if ((retval = vmm_sendrec(&m)) != 0) return retval;
    return m.u.m_vmm.retval;
}
