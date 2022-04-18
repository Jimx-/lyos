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
    along with Lyos.  If not, see <http://www.gnu.org/licenses/". */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/const.h"
#include "lyos/vm.h"

int syscall_entry(int syscall_nr, MESSAGE* m);

int vmctl(int request, endpoint_t who)
{
    MESSAGE m;

    m.VMCTL_REQUEST = request;
    m.VMCTL_WHO = who;

    return syscall_entry(NR_VMCTL, &m);
}

int vmctl_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_GET_KERN_MAPPING;
    m.VMCTL_GET_KM_INDEX = index;
    syscall_entry(NR_VMCTL, &m);

    *addr = m.VMCTL_GET_KM_ADDR;
    *len = m.VMCTL_GET_KM_LEN;
    *flags = m.VMCTL_GET_KM_FLAGS;

    return m.VMCTL_GET_KM_RETVAL;
}

int vmctl_reply_kern_mapping(int index, void* vir_addr)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_REPLY_KERN_MAPPING;
    m.VMCTL_REPLY_KM_INDEX = index;
    m.VMCTL_REPLY_KM_ADDR = vir_addr;

    syscall_entry(NR_VMCTL, &m);

    return m.VMCTL_REPLY_KM_RETVAL;
}

int vmctl_getpdbr(endpoint_t who, unsigned long* pdbr)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_GETPDBR;
    m.VMCTL_WHO = who;

    int retval = syscall_entry(NR_VMCTL, &m);
    if (retval) return retval;

    *pdbr = m.VMCTL_PHYS_ADDR;

    return 0;
}

int vmctl_set_address_space(endpoint_t who, unsigned long pgd_phys)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_SET_ADDRESS_SPACE;
    m.VMCTL_WHO = who;
    m.VMCTL_PHYS_ADDR = pgd_phys;
    /* m.VMCTL_VIR_ADDR = pgd_vir; */

    return syscall_entry(NR_VMCTL, &m);
}

int vmctl_flushtlb(endpoint_t who)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_FLUSHTLB;
    m.VMCTL_WHO = who;

    return syscall_entry(NR_VMCTL, &m);
}

int vmctl_get_mmrequest(endpoint_t* target, void** start, size_t* len,
                        int* flags, endpoint_t* caller)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_GET_MMREQ;

    int retval = syscall_entry(NR_VMCTL, &m);

    *target = m.VMCTL_MMREQ_TARGET;
    *start = m.VMCTL_MMREQ_ADDR;
    *len = m.VMCTL_MMREQ_LEN;
    *flags = m.VMCTL_MMREQ_FLAGS;
    *caller = m.VMCTL_MMREQ_CALLER;

    return retval;
}

int vmctl_reply_mmreq(endpoint_t who, int result)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_REPLY_MMREQ;
    m.VMCTL_WHO = who;
    m.VMCTL_VALUE = result;

    return syscall_entry(NR_VMCTL, &m);
}

#ifdef __aarch64__

int vmctl_getkpdbr(endpoint_t who, unsigned long* kpdbr)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_GETKPDBR;
    m.VMCTL_WHO = who;

    int retval = syscall_entry(NR_VMCTL, &m);
    if (retval) return retval;

    *kpdbr = m.VMCTL_PHYS_ADDR;

    return 0;
}

int vmctl_set_kern_address_space(unsigned long pgd_phys)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_SET_KADDRSPACE;
    m.VMCTL_PHYS_ADDR = pgd_phys;

    return syscall_entry(NR_VMCTL, &m);
}

#endif
