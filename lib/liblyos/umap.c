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

PUBLIC int umap(endpoint_t ep, void * vir_addr, phys_bytes * phys_addr)
{
    MESSAGE m;
    m.UMAP_WHO = ep;
    m.UMAP_SRCADDR = vir_addr;

    int ret = syscall_entry(NR_UMAP, &m);
    if (ret) return ret;

    *phys_addr = (phys_bytes)m.UMAP_DSTADDR;

    return 0;
}
