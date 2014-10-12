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

PUBLIC  int vmctl(int request, int param);

PUBLIC int vmctl_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    MESSAGE m;
    m.VMCTL_GET_KM_INDEX = index;
    vmctl(VMCTL_GET_KERN_MAPPING, (int)&m);

    *addr = m.VMCTL_GET_KM_ADDR;
    *len = m.VMCTL_GET_KM_LEN;
    *flags = m.VMCTL_GET_KM_FLAGS;

    return m.VMCTL_GET_KM_RETVAL;
}

PUBLIC int vmctl_reply_kern_mapping(int index, void * vir_addr)
{
    MESSAGE m;
    
    m.VMCTL_REPLY_KM_INDEX = index;
    m.VMCTL_REPLY_KM_ADDR = vir_addr;

    vmctl(VMCTL_REPLY_KERN_MAPPING, (int)&m);

    return m.VMCTL_REPLY_KM_RETVAL;
}
