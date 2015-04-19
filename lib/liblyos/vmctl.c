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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/const.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/vm.h"
#include "lyos/ipc.h"

int syscall_entry(int syscall_nr, MESSAGE * m);

PUBLIC int vmctl(int request, endpoint_t who)
{
    MESSAGE m;

    m.VMCTL_REQUEST = request;
    m.VMCTL_WHO = who;

    return syscall_entry(NR_VMCTL, &m);
}

PUBLIC int vmctl_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
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

PUBLIC int vmctl_reply_kern_mapping(int index, void * vir_addr)
{
    MESSAGE m;
    
    m.VMCTL_REQUEST = VMCTL_REPLY_KERN_MAPPING;
    m.VMCTL_REPLY_KM_INDEX = index;
    m.VMCTL_REPLY_KM_ADDR = vir_addr;

    syscall_entry(NR_VMCTL, &m);

    return m.VMCTL_REPLY_KM_RETVAL;
}

PUBLIC int vmctl_getpdbr(endpoint_t who, unsigned * pdbr)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_GETPDBR;
    m.VMCTL_WHO = who;

    int retval = syscall_entry(NR_VMCTL, &m);
    if (retval) return retval;

    *pdbr = m.VMCTL_VALUE;

    return 0;
}

PUBLIC int vmctl_set_address_space(endpoint_t who, void * pgd_phys, void * pgd_vir)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_SET_ADDRESS_SPACE;
    m.VMCTL_WHO = who;
    m.VMCTL_PHYS_ADDR = pgd_phys;
    m.VMCTL_VIR_ADDR = pgd_vir;

    return syscall_entry(NR_VMCTL, &m);
}

PUBLIC int vmctl_get_mmrequest(endpoint_t * target, vir_bytes * start, vir_bytes * len, 
                        int * flags, endpoint_t * caller)
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

PUBLIC int vmctl_reply_mmreq(endpoint_t who, int result)
{
    MESSAGE m;

    m.VMCTL_REQUEST = VMCTL_REPLY_MMREQ;
    m.VMCTL_WHO = who;
    m.VMCTL_VALUE = result;

    return syscall_entry(NR_VMCTL, &m);
}
