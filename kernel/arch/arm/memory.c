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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>
#include <lyos/smp.h>

PUBLIC void init_memory()
{

}

PUBLIC void clear_memcache()
{
    
}

PUBLIC void * va2pa(endpoint_t ep, void * va)
{
    return NULL;
}

PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{

}

PUBLIC int arch_reply_kern_mapping(int index, void * vir_addr)
{

}

PUBLIC int arch_vmctl(MESSAGE * m, struct proc * p)
{

}

PUBLIC int _vir_copy(struct proc * caller, struct vir_addr * dest_addr, struct vir_addr * src_addr,
                                vir_bytes bytes, int check)
{

}

PUBLIC int _data_vir_copy(struct proc * caller, endpoint_t dest_ep, void * dest_addr,
                        endpoint_t src_ep, void * src_addr, int len, int check)
{

}
