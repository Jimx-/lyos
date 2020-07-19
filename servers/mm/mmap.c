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

#include <lyos/types.h>
#include <lyos/ipc.h>
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
#include <sys/mman.h>
#include "global.h"

struct vir_region* mmap_region(struct mmproc* mmp, void* addr, int mmap_flags,
                               size_t len, int vrflags)
{
    struct vir_region* vr = NULL;

    if (mmap_flags & MAP_PRIVATE)
        vrflags |= RF_PRIVATE;
    else if (mmap_flags & MAP_SHARED)
        vrflags |= RF_SHARED;
    if (mmap_flags & MAP_CONTIG) vrflags |= RF_CONTIG;

    /* Contiguous physical memory must be populated */
    if ((mmap_flags & (MAP_POPULATE | MAP_CONTIG)) == MAP_CONTIG) {
        return NULL;
    }

    len = roundup(len, ARCH_PG_SIZE);

    /* first unmap the region */
    if (addr && (mmap_flags & MAP_FIXED)) {
        if (region_unmap_range(mmp, (vir_bytes)addr, len)) return NULL;
    }

    if (addr || (mmap_flags & MAP_FIXED)) {
        vr = region_find_free_region(mmp, (vir_bytes)addr, 0, len, vrflags);
        if (!vr && (mmap_flags & MAP_FIXED)) return NULL;
    } else {
        vr = region_find_free_region(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len,
                                     vrflags);
        if (!vr) return NULL;
    }

    if (mmap_flags &
        MAP_POPULATE) { /*  populate (prefault) page tables for a mapping */
        region_alloc_phys(vr);
        region_map_phys(mmp, vr);
    }

    return vr;
}

static int mmap_file(struct mmproc* mmp, void* addr, size_t len, int flags,
                     int prot, int mmfd, off_t offset, dev_t dev, ino_t ino,
                     size_t clearend, void** ret_addr)
{
    int vrflags = 0;
    struct vir_region* vr;

    len = roundup(len, ARCH_PG_SIZE);

    if (prot & PROT_WRITE) vrflags |= RF_WRITABLE;

    if (offset % ARCH_PG_SIZE) return EINVAL;
    if ((vir_bytes)addr % ARCH_PG_SIZE) return EINVAL;

    if ((vr = mmap_region(mmp, addr, flags, len, vrflags)) == NULL)
        return ENOMEM;
    list_add(&vr->list, &mmp->mm->mem_regions);
    avl_insert(&vr->avl, &mmp->mm->mem_avl);

    *ret_addr = (void*)vr->vir_addr;

    struct mm_file_desc* filp = get_mm_file_desc(mmfd, dev, ino);
    if (!filp) return ENOMEM;

    file_reference(vr, filp);
    vr->flags |= RF_FILEMAP;
    vr->param.file.offset = offset;
    vr->param.file.clearend = clearend;

    return 0;
}

static void mmap_device_callback(struct mmproc* mmp, MESSAGE* msg, void* arg)
{
    /* driver has done the mapping */
    enqueue_vfs_request(&mmproc_table[TASK_MM], MMR_FDCLOSE, msg->MMRFD, 0, 0,
                        0, NULL, NULL, 0);

    MESSAGE reply_msg;
    memset(&reply_msg, 0, sizeof(MESSAGE));
    reply_msg.type = SYSCALL_RET;
    reply_msg.u.m_mm_mmap_reply.retval = msg->MMRRESULT;
    reply_msg.u.m_mm_mmap_reply.retaddr = msg->MMRBUF;

    send_recv(SEND_NONBLOCK, msg->MMRENDPOINT, &reply_msg);
}

static void mmap_file_callback(struct mmproc* mmp, MESSAGE* msg, void* arg)
{
    MESSAGE* mmap_msg = (MESSAGE*)arg;
    void* ret_addr = MAP_FAILED;
    int result;

    if (msg->MMRRESULT) {
        result = msg->MMRRESULT;
    } else {
        mode_t file_mode = msg->MMRMODE;
        if ((file_mode & I_TYPE) == I_CHAR_SPECIAL) { /* map device */
            if (enqueue_vfs_request(
                    mmp, MMR_FDMMAP, msg->MMRFD, mmap_msg->u.m_mm_mmap.vaddr,
                    mmap_msg->u.m_mm_mmap.offset, mmap_msg->u.m_mm_mmap.length,
                    mmap_device_callback, mmap_msg, sizeof(MESSAGE)) != 0) {
                result = ENOMEM;
            } else {
                return;
            }
        } else {
            result = mmap_file(
                mmp, mmap_msg->u.m_mm_mmap.vaddr, mmap_msg->u.m_mm_mmap.length,
                mmap_msg->u.m_mm_mmap.flags, mmap_msg->u.m_mm_mmap.prot,
                msg->MMRFD, mmap_msg->u.m_mm_mmap.offset, msg->MMRDEV,
                msg->MMRINO, 0, &ret_addr);
        }
    }

    MESSAGE reply_msg;
    memset(&reply_msg, 0, sizeof(MESSAGE));
    reply_msg.type = SYSCALL_RET;
    reply_msg.u.m_mm_mmap_reply.retval = result;
    reply_msg.u.m_mm_mmap_reply.retaddr = ret_addr;

    send_recv(SEND_NONBLOCK, mmap_msg->source, &reply_msg);
}

int do_vfs_mmap()
{
    endpoint_t who = mm_msg.u.m_mm_mmap.who;
    struct mmproc* mmp = endpt_mmproc(who);
    if (!mmp) return ESRCH;

    void* ret_addr;
    return mmap_file(mmp, mm_msg.u.m_mm_mmap.vaddr, mm_msg.u.m_mm_mmap.length,
                     mm_msg.u.m_mm_mmap.flags, mm_msg.u.m_mm_mmap.prot,
                     mm_msg.u.m_mm_mmap.fd, mm_msg.u.m_mm_mmap.offset,
                     mm_msg.u.m_mm_mmap.devino.dev,
                     mm_msg.u.m_mm_mmap.devino.ino, mm_msg.u.m_mm_mmap.clearend,
                     &ret_addr);
}

int do_mmap()
{
    endpoint_t who =
        mm_msg.u.m_mm_mmap.who < 0 ? mm_msg.source : mm_msg.u.m_mm_mmap.who;
    void* addr = mm_msg.u.m_mm_mmap.vaddr;
    size_t len = mm_msg.u.m_mm_mmap.length;
    struct mmproc* mmp = endpt_mmproc(who);
    int flags = mm_msg.u.m_mm_mmap.flags;
    int fd = mm_msg.u.m_mm_mmap.fd;
    int prot = mm_msg.u.m_mm_mmap.prot;

    struct vir_region* vr = NULL;

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
        list_add(&vr->list, &mmp->mm->mem_regions);
        avl_insert(&vr->avl, &mmp->mm->mem_avl);
    } else { /* mapping file */
        if (enqueue_vfs_request(mmp, MMR_FDLOOKUP, fd, 0, 0, 0,
                                mmap_file_callback, &mm_msg,
                                sizeof(MESSAGE)) != 0)
            return ENOMEM;

        return SUSPEND;
    }

    mm_msg.u.m_mm_mmap_reply.retaddr = (void*)vr->vir_addr;
    return 0;
}

static int map_perm_check(endpoint_t source, endpoint_t target,
                          phys_bytes phys_addr, phys_bytes len)
{
    /* if (source == TASK_TTY) return 0;
    return EPERM; */
    return 0;
}

int do_map_phys()
{
    endpoint_t who = mm_msg.ENDPOINT == SELF ? mm_msg.source : mm_msg.ENDPOINT;
    phys_bytes phys_addr = (phys_bytes)mm_msg.ADDR;
    phys_bytes len = mm_msg.BUF_LEN;
    struct mmproc* mmp = endpt_mmproc(who);
    int retval = 0;

    if (!mmp) return EINVAL;

    if ((retval = map_perm_check(mm_msg.source, who, phys_addr, len)) != 0)
        return retval;

    /* align */
    off_t offset = phys_addr % ARCH_PG_SIZE;
    phys_addr -= offset;
    len += offset;
    if (len % ARCH_PG_SIZE) len += ARCH_PG_SIZE - (len % ARCH_PG_SIZE);

    struct vir_region* vr = region_find_free_region(
        mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len, RF_WRITABLE | RF_DIRECT);
    if (!vr) return ENOMEM;
    list_add(&vr->list, &mmp->mm->mem_regions);
    avl_insert(&vr->avl, &mmp->mm->mem_avl);

    region_set_phys(vr, phys_addr);
    region_map_phys(mmp, vr);

    mm_msg.ADDR = (void*)(vr->vir_addr + offset);

    return 0;
}

int do_munmap()
{
    endpoint_t who =
        mm_msg.u.m_mm_mmap.who < 0 ? mm_msg.source : mm_msg.u.m_mm_mmap.who;
    void* addr = mm_msg.u.m_mm_mmap.vaddr;
    size_t len = mm_msg.u.m_mm_mmap.length;
    struct mmproc* mmp = endpt_mmproc(who);

    if (len < 0) return EINVAL;
    if (!mmp) return EINVAL;

    len = roundup(len, ARCH_PG_SIZE);

    return region_unmap_range(mmp, (vir_bytes)addr, len);
}

int do_mm_remap()
{
    endpoint_t src = mm_msg.u.m_mm_remap.src;
    if (src == SELF) src = mm_msg.source;
    endpoint_t dest = mm_msg.u.m_mm_remap.dest;
    if (src == SELF) dest = mm_msg.source;
    void* src_addr = mm_msg.u.m_mm_remap.src_addr;
    void* dest_addr = mm_msg.u.m_mm_remap.dest_addr;
    size_t size = mm_msg.u.m_mm_remap.size;

    if (size < 0) return EINVAL;
    struct mmproc* smmp = endpt_mmproc(src);
    if (!smmp) return EINVAL;
    struct mmproc* dmmp = endpt_mmproc(dest);
    if (!dmmp) return EINVAL;

    struct vir_region* src_region = region_lookup(smmp, (vir_bytes)src_addr);
    if (src_region->vir_addr != (vir_bytes)src_addr) return EFAULT;

    if (size % ARCH_PG_SIZE) {
        size = size - (size % ARCH_PG_SIZE) + ARCH_PG_SIZE;
    }

    if (size != src_region->length) return EFAULT;

    int vrflags = RF_SHARED | RF_WRITABLE;
    struct vir_region* new_region = NULL;
    if (dest_addr) {
        new_region = region_new((vir_bytes)dest_addr, size, vrflags);
    } else {
        new_region = region_find_free_region(dmmp, ARCH_BIG_PAGE_SIZE,
                                             VM_STACK_TOP, size, vrflags);
    }
    if (!new_region) return ENOMEM;

    region_share(dmmp, new_region, smmp, src_region, TRUE);
    region_map_phys(dmmp, new_region);

    mm_msg.u.m_mm_remap.ret_addr = (void*)new_region->vir_addr;
    return 0;
}
