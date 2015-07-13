/*  
    (c)Copyright 2011 Jimx
    
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
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/param.h>
#include <lyos/sysutils.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <lyos/vm.h>
#include <libmemfs/libmemfs.h>
#include "type.h"
#include "proto.h"

PUBLIC void root_meminfo()
{
    struct mem_info mem_info;

    if (get_meminfo(&mem_info) != 0) buf_printf("Memory information not available");
    else {
        buf_printf("MemTotal:     %10d kB\n", mem_info.mem_total / 1024);
        buf_printf("MemFree:      %10d kB\n", mem_info.mem_free / 1024);
        buf_printf("Cached:       %10d kB\n", mem_info.cached / 1024);
        buf_printf("Slab:         %10d kB\n", mem_info.slab / 1024);
        buf_printf("VmallocTotal: %10d kB\n", mem_info.vmalloc_total / 1024);
        buf_printf("VmallocUsed:  %10d kB\n", mem_info.vmalloc_used / 1024);
    }
}
