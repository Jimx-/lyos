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
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/keyboard.h"
#include "lyos/proto.h"
#include "region.h"
#include "proto.h"
#include "const.h"
#include "lyos/vm.h"
#include <sys/mman.h>
#include "global.h"

PRIVATE struct vir_region * mmap_region(struct proc * p, int addr,
    int mmap_flags, size_t len, int vrflags)
{
    struct mmproc * mmp = mmproc_table + (p - proc_table);
    struct vir_region * vr = NULL;
    if (addr || (mmap_flags & MAP_FIXED)) {
        vr = region_new(p, (void *)addr, len, vrflags);
        if(!vr && (mmap_flags & MAP_FIXED))
            return NULL;
        region_alloc_phys(vr);
        region_map_phys(p, vr);

        if (addr && (mmap_flags & MAP_GROWSDOWN)) {
            /* create a growsdown guard region */
            struct vir_region * guard_region = region_new(p, (void*)(addr - GROWSDOWN_GUARD_LEN), GROWSDOWN_GUARD_LEN, RF_GUARD);
            list_add(&(guard_region->list), &(mmp->mem_regions));
        }
    }

    return vr;
}

PUBLIC int do_mmap()
{
    endpoint_t who = mm_msg.MMAP_WHO < 0 ? mm_msg.source : mm_msg.MMAP_WHO;
    int addr = mm_msg.MMAP_VADDR;
    size_t len = mm_msg.MMAP_LEN;
    struct proc * p = proc_table + who;
    struct mmproc * mmp = mmproc_table + who;
    int flags = mm_msg.MMAP_FLAGS;
    int fd = mm_msg.MMAP_FD;
    int prot = mm_msg.MMAP_PROT;

    struct vir_region * vr = NULL;

    if (len < 0) return EINVAL;

    if ((fd == -1) || (flags & MAP_ANONYMOUS)) {
        if (fd != -1) return EINVAL;

        int vr_flags = RF_NORMAL;

        if (prot & PROT_WRITE) vr_flags |= RF_WRITABLE;

        if (flags & MAP_GROWSDOWN) vr_flags |= RF_GROWSDOWN;
        if (!(vr = mmap_region(p, addr, flags, len, vr_flags))) return ENOMEM;
        list_add(&(vr->list), &(mmp->mem_regions));
    }

    mm_msg.MMAP_RETADDR = (int)vr->vir_addr;
    return 0;
}
