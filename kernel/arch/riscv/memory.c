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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include "lyos/cpulocals.h"
#include <lyos/param.h>
#include <lyos/vm.h>
#include <errno.h>
#include <lyos/smp.h>

extern int _KERN_OFFSET;

/* temporary mappings */
#define MAX_TEMPPDES    2
#define TEMPPDE_SRC     0
#define TEMPPDE_DST     1
PRIVATE u32 temppdes[MAX_TEMPPDES];

#define _SRC_           0
#define _DEST_          1
#define EFAULT_SRC      1
#define EFAULT_DEST     2

PUBLIC void init_memory()
{
    int i;
    for (i = 0; i < MAX_TEMPPDES; i++) {
        temppdes[i] = kinfo.kernel_end_pde++;
    }
    get_cpulocal_var(pt_proc) = proc_addr(TASK_MM);
}

PUBLIC void clear_memcache()
{
}

/* Temporarily map la in p's address space in kernel address space */
PRIVATE phys_bytes create_temp_map(struct proc * p, phys_bytes la, phys_bytes * len, int index, int * changed)
{
}

PRIVATE int la_la_copy(struct proc * p_dest, phys_bytes dest_la,
        struct proc * p_src, phys_bytes src_la, void* len)
{
}

PRIVATE u32 get_phys32(phys_bytes phys_addr)
{
}

PUBLIC void * va2pa(endpoint_t ep, void * va)
{
}

#define MAX_KERN_MAPPINGS   8

struct kern_map {
    phys_bytes phys_addr;
    phys_bytes len;
    int flags;
    void* vir_addr;
    void** mapped_addr;
};

PRIVATE struct kern_map kern_mappings[MAX_KERN_MAPPINGS];
PRIVATE int kern_mapping_count = 0;

PUBLIC int kern_map_phys(phys_bytes phys_addr, phys_bytes len, int flags, void** mapped_addr)
{
    return 0;
}

#define KM_USERMAPPED   0
#define KM_KERN_MAPPING 1

extern char _usermapped[], _eusermapped[];
PUBLIC off_t usermapped_offset;

PUBLIC int arch_get_kern_mapping(int index, caddr_t * addr, int * len, int * flags)
{
    return 0;
}

PUBLIC int arch_reply_kern_mapping(int index, void * vir_addr)
{
    return 0;
}

PUBLIC int arch_vmctl(MESSAGE * m, struct proc * p)
{
}

PUBLIC void mm_suspend(struct proc * caller, endpoint_t target, void* laddr, void* bytes, int write, int type)
{
}

PUBLIC int _vir_copy(struct proc * caller, struct vir_addr * dest_addr, struct vir_addr * src_addr,
                                size_t bytes, int check)
{
    return 0;
}

PUBLIC int _data_vir_copy(struct proc * caller, endpoint_t dest_ep, void * dest_addr,
                        endpoint_t src_ep, void * src_addr, int len, int check)
{
}
