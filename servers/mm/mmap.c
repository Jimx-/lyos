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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "region.h"
#include "proto.h"
#include "const.h"
#include "lyos/vm.h"
#include <sys/mman.h>
#include "global.h"

PUBLIC struct vir_region * mmap_region(struct mmproc * mmp, int addr,
    int mmap_flags, size_t len, int vrflags)
{
    struct vir_region * vr = NULL;
    if (addr || (mmap_flags & MAP_FIXED)) {
        vr = region_new((void *)addr, len, vrflags);
        if(!vr && (mmap_flags & MAP_FIXED))
            return NULL;

        if (!(mmap_flags & MAP_PREALLOC)) {
            region_alloc_phys(vr);
            region_map_phys(mmp, vr);
        }
    }

    return vr;
}

PUBLIC int do_mmap()
{
    endpoint_t who = mm_msg.MMAP_WHO < 0 ? mm_msg.source : mm_msg.MMAP_WHO;
    int addr = mm_msg.MMAP_VADDR;
    size_t len = mm_msg.MMAP_LEN;
    struct mmproc * mmp = endpt_mmproc(who);
    int flags = mm_msg.MMAP_FLAGS;
    int fd = mm_msg.MMAP_FD;
    int prot = mm_msg.MMAP_PROT;

    struct vir_region * vr = NULL;

    if (len < 0) return EINVAL;

    if ((fd == -1) || (flags & MAP_ANONYMOUS)) {
        if (fd != -1) return EINVAL;

        int vr_flags = RF_NORMAL;

        if (prot & PROT_WRITE) vr_flags |= RF_WRITABLE;

        if (!(vr = mmap_region(mmp, addr, flags, len, vr_flags))) return ENOMEM;
        list_add(&(vr->list), &mmp->active_mm->mem_regions);
    }

    mm_msg.MMAP_RETADDR = (int)vr->vir_addr;
    return 0;
}

PRIVATE int map_perm_check(endpoint_t source, endpoint_t target, phys_bytes phys_addr, phys_bytes len)
{
    if (source == TASK_TTY) return 0;
    return EPERM;
}

PUBLIC int do_map_phys()
{
    endpoint_t who = mm_msg.ENDPOINT == SELF ? mm_msg.source : mm_msg.ENDPOINT;
    phys_bytes phys_addr = (phys_bytes)mm_msg.ADDR;
    phys_bytes len = mm_msg.BUF_LEN;
    struct mmproc * mmp = endpt_mmproc(who);
    int retval = 0;

    if (!mmp) return EINVAL;

    if ((retval = map_perm_check(mm_msg.source, who, phys_addr, len)) != 0) return retval;

    /* align */
    off_t offset = phys_addr % ARCH_PG_SIZE;
    phys_addr -= offset;
    len += offset;
    if (len % ARCH_PG_SIZE) len += ARCH_PG_SIZE - (len % ARCH_PG_SIZE);

    struct vir_region * vr = region_find_free_region(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len, RF_WRITABLE);
    if (!vr) return ENOMEM;

    region_set_phys(vr, phys_addr);
    region_map_phys(mmp, vr);

    mm_msg.ADDR = (void *)((vir_bytes)vr->vir_addr + offset);

    return 0;
}
