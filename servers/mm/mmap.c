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
#include <lyos/fs.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include "lyos/vm.h"
#include <lyos/ipc.h>
#include <sys/mman.h>
#include "global.h"

PUBLIC struct vir_region * mmap_region(struct mmproc * mmp, int addr,
    int mmap_flags, size_t len, int vrflags)
{
    struct vir_region * vr = NULL;

    if (mmap_flags & MAP_PRIVATE) vrflags |= RF_PRIVATE;
    else if (mmap_flags & MAP_SHARED) vrflags |= RF_SHARED;

    if (addr || (mmap_flags & MAP_FIXED)) {
        //vr = region_new((void *)addr, len, vrflags);
        vr = region_find_free_region(mmp, addr, 0, len, vrflags);
        if(!vr && (mmap_flags & MAP_FIXED))
            return NULL;
    } else {
        vr = region_find_free_region(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len, vrflags);
        if (!vr) return NULL;
    }

    if (mmap_flags & MAP_POPULATE) {    /*  populate (prefault) page tables for a mapping */
        region_alloc_phys(vr);
        region_map_phys(mmp, vr);
    }

    return vr;
}

PRIVATE int mmap_file(struct mmproc* mmp, vir_bytes addr, vir_bytes len, int flags, int prot, int mmfd, off_t offset, dev_t dev, ino_t ino, size_t clearend, vir_bytes* ret_addr)
{
    int vrflags = 0;
    struct vir_region * vr;

    len = roundup(len, ARCH_PG_SIZE);

    if (prot & PROT_WRITE) vrflags |= RF_WRITABLE;

    if (offset % ARCH_PG_SIZE) return EINVAL;
    if (addr % ARCH_PG_SIZE) return EINVAL;

    if ((vr = mmap_region(mmp, addr, flags, len, vrflags)) == NULL) return ENOMEM;
    list_add(&(vr->list), &mmp->active_mm->mem_regions);
    avl_insert(&vr->avl, &mmp->active_mm->mem_avl);

    *ret_addr = (vir_bytes)vr->vir_addr;

    struct mm_file_desc* filp = get_mm_file_desc(mmfd, dev, ino);
    if (!filp) return ENOMEM;

    file_reference(vr, filp);
    vr->flags |= RF_FILEMAP;
    vr->param.file.offset = offset;
    vr->param.file.clearend = clearend;

    return 0;
}

PRIVATE void mmap_file_callback(struct mmproc* mmp, MESSAGE* msg, void* arg)
{
    MESSAGE* mmap_msg = (MESSAGE*) arg;
    vir_bytes ret_addr = (vir_bytes) MAP_FAILED;
    int result;

    if (msg->MMRRESULT) {
        result = msg->MMRRESULT;
    } else {
        result = mmap_file(mmp, (vir_bytes)mmap_msg->MMAP_VADDR, mmap_msg->MMAP_LEN, mmap_msg->MMAP_FLAGS, mmap_msg->MMAP_PROT, msg->MMRFD, 
                mmap_msg->MMAP_OFFSET, msg->MMRDEV, msg->MMRINO, 0, &ret_addr);
    }

    MESSAGE reply_msg;
    memset(&reply_msg, 0, sizeof(MESSAGE));
    reply_msg.type = SYSCALL_RET;
    reply_msg.RETVAL = result;
    reply_msg.MMAP_RETADDR = ret_addr;

    send_recv(SEND_NONBLOCK, mmap_msg->source, &reply_msg);
}

PUBLIC int do_vfs_mmap()
{
    endpoint_t src = mm_msg.source;

    endpoint_t who = mm_msg.MMAP_WHO;
    struct mmproc* mmp = endpt_mmproc(who);
    if (!mmp) return ESRCH;

    vir_bytes ret_addr;
    return mmap_file(mmp, mm_msg.MMAP_VADDR, mm_msg.MMAP_LEN, mm_msg.MMAP_FLAGS, mm_msg.MMAP_PROT, mm_msg.MMAP_FD, 
            mm_msg.MMAP_OFFSET, mm_msg.MMAP_DEV, mm_msg.MMAP_INO, mm_msg.MMAP_CLEAREND, &ret_addr);
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

    /* return EINVAL when flags contained neither MAP_PRIVATE or MAP_SHARED, or
     * contained both of these values. */
    if ((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) return EINVAL;
    if (!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED)) return EINVAL;

    if ((fd == -1) || (flags & MAP_ANONYMOUS)) {
        if (fd != -1) return EINVAL;

        int vr_flags = RF_NORMAL;

        if (prot & PROT_WRITE) vr_flags |= RF_WRITABLE;

        if (!(vr = mmap_region(mmp, addr, flags, len, vr_flags))) return ENOMEM;
        list_add(&(vr->list), &mmp->active_mm->mem_regions);
        avl_insert(&vr->avl, &mmp->active_mm->mem_avl);
    } else {    /* mapping file */
        if (enqueue_vfs_request(mmp, MMR_FDLOOKUP, fd, 0, 0, 0, mmap_file_callback, &mm_msg, sizeof(MESSAGE)) != 0) return ENOMEM;

        return SUSPEND;
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
    list_add(&vr->list, &mmp->active_mm->mem_regions);
    avl_insert(&vr->avl, &mmp->active_mm->mem_avl);

    region_set_phys(vr, phys_addr);
    region_map_phys(mmp, vr);

    mm_msg.ADDR = (void *)((vir_bytes)vr->vir_addr + offset);

    return 0;
}

PUBLIC int do_munmap()
{
    endpoint_t who = mm_msg.MMAP_WHO < 0 ? mm_msg.source : mm_msg.MMAP_WHO;
    vir_bytes addr = mm_msg.MMAP_VADDR;
    size_t len = mm_msg.MMAP_LEN;
    struct mmproc * mmp = endpt_mmproc(who);

    if (len < 0) return EINVAL;
    if (!mmp) return EINVAL;

    struct vir_region * vr;
    vr = region_lookup(mmp, addr);

    if (!vr) return EINVAL;

    /* TODO: split region */
    region_unmap_phys(mmp, vr);
    list_del(&vr->list);
    avl_erase(&vr->avl, &mmp->active_mm->mem_avl);
    region_free(vr);

    return 0;
}
