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
#include <sys/stat.h>

#include "region.h"
#include "proto.h"
#include "const.h"
#include "lyos/vm.h"
#include <sys/mman.h>
#include "global.h"

struct vir_region* mmap_region(struct mmproc* mmp, vir_bytes addr,
                               int mmap_flags, size_t len, int vrflags,
                               const struct region_operations* rops)
{
    struct vir_region* vr = NULL;
    int mrflags = 0;

    if (mmap_flags & MAP_POPULATE) mrflags |= MRF_PREALLOC;

    if (len <= 0) return NULL;

    len = roundup(len, ARCH_PG_SIZE);

    /* first unmap the region */
    if (addr && (mmap_flags & MAP_FIXED)) {
        if (region_unmap_range(mmp, (vir_bytes)addr, len)) return NULL;
    }

    if (addr || (mmap_flags & MAP_FIXED)) {
        vr = region_map(mmp, addr, 0, len, vrflags, mrflags, rops);
        if (!vr && (mmap_flags & MAP_FIXED)) return NULL;
    }

    if (!vr) {
        vr = region_map(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len, vrflags,
                        mrflags, rops);
    }

    return vr;
}

static int mmap_file(struct mmproc* mmp, vir_bytes addr, size_t len, int flags,
                     int prot, int mmfd, off_t offset, dev_t dev, ino_t ino,
                     size_t clearend, int may_close, vir_bytes* ret_addr)
{
    int vrflags = 0;
    struct vir_region* vr;
    size_t page_off;
    int retval = 0;

    if (prot & PROT_WRITE) vrflags |= RF_WRITABLE;
    if (flags & MAP_SHARED) vrflags |= RF_MAP_SHARED;

    if ((page_off = (offset % ARCH_PG_SIZE)) != 0) {
        offset -= page_off;
        len += page_off;
    }

    len = roundup(len, ARCH_PG_SIZE);

    assert(!(len % ARCH_PG_SIZE));
    assert(!(offset % ARCH_PG_SIZE));

    if ((vr = mmap_region(mmp, addr, flags, len, vrflags, &file_map_ops)) ==
        NULL) {
        retval = ENOMEM;
    } else {
        *ret_addr = vr->vir_addr + page_off;

        retval = file_map_set_file(mmp, vr, mmfd, offset, dev, ino, clearend,
                                   TRUE, may_close);
    }

    return retval;
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

static void mmap_file_callback(struct mmproc* mmp, MESSAGE* reply_msg,
                               void* arg)
{
    MESSAGE* mmap_msg = (MESSAGE*)arg;
    vir_bytes ret_addr = (vir_bytes)MAP_FAILED;
    MESSAGE user_reply;
    int result;

    if (reply_msg->MMRRESULT != OK) {
        result = reply_msg->MMRRESULT;
    } else {
        mode_t file_mode = reply_msg->MMRMODE;
        if (S_ISCHR(file_mode)) { /* map device */
            if (enqueue_vfs_request(
                    mmp, MMR_FDMMAP, reply_msg->MMRFD,
                    mmap_msg->u.m_mm_mmap.vaddr, mmap_msg->u.m_mm_mmap.offset,
                    mmap_msg->u.m_mm_mmap.length, mmap_device_callback,
                    mmap_msg, sizeof(MESSAGE)) != 0) {
                result = ENOMEM;
            } else {
                return;
            }
        } else {
            result = mmap_file(mmp, (vir_bytes)mmap_msg->u.m_mm_mmap.vaddr,
                               mmap_msg->u.m_mm_mmap.length,
                               mmap_msg->u.m_mm_mmap.flags,
                               mmap_msg->u.m_mm_mmap.prot, reply_msg->MMRFD,
                               mmap_msg->u.m_mm_mmap.offset, reply_msg->MMRDEV,
                               reply_msg->MMRINO, 0, TRUE, &ret_addr);
        }
    }

    memset(&user_reply, 0, sizeof(MESSAGE));
    user_reply.type = SYSCALL_RET;
    user_reply.u.m_mm_mmap_reply.retval = result;
    user_reply.u.m_mm_mmap_reply.retaddr = (void*)ret_addr;

    send_recv(SEND_NONBLOCK, mmap_msg->source, &user_reply);
}

int do_vfs_mmap()
{
    endpoint_t who = mm_msg.u.m_mm_mmap.who;
    vir_bytes ret_addr;
    struct mmproc* mmp = endpt_mmproc(who);

    if (mm_msg.source != TASK_FS) return EPERM;

    if (!mmp) return ESRCH;

    return mmap_file(mmp, (vir_bytes)mm_msg.u.m_mm_mmap.vaddr,
                     mm_msg.u.m_mm_mmap.length, mm_msg.u.m_mm_mmap.flags,
                     mm_msg.u.m_mm_mmap.prot, mm_msg.u.m_mm_mmap.fd,
                     mm_msg.u.m_mm_mmap.offset, mm_msg.u.m_mm_mmap.dev,
                     mm_msg.u.m_mm_mmap.ino, mm_msg.u.m_mm_mmap.clearend, FALSE,
                     &ret_addr);
}

int do_mmap()
{
    endpoint_t who =
        mm_msg.u.m_mm_mmap.who < 0 ? mm_msg.source : mm_msg.u.m_mm_mmap.who;
    vir_bytes addr = (vir_bytes)mm_msg.u.m_mm_mmap.vaddr;
    size_t len = mm_msg.u.m_mm_mmap.length;
    struct mmproc* mmp = endpt_mmproc(who);
    int flags = mm_msg.u.m_mm_mmap.flags;
    int fd = mm_msg.u.m_mm_mmap.fd;
    int prot = mm_msg.u.m_mm_mmap.prot;
    struct vir_region* vr = NULL;
    int vr_flags = 0;
    const struct region_operations* rops = NULL;

    if (len <= 0) return EINVAL;

    if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0 ||
        (flags & (MAP_PRIVATE | MAP_SHARED)) == (MAP_PRIVATE | MAP_SHARED))
        return EINVAL;

    if ((fd == -1) || (flags & MAP_ANONYMOUS)) {
        if (fd != -1) return EINVAL;

        if ((flags & (MAP_CONTIG | MAP_POPULATE)) == MAP_CONTIG) return EINVAL;

        if (prot & PROT_WRITE) vr_flags |= RF_WRITABLE;
        if (flags & MAP_SHARED) vr_flags |= RF_MAP_SHARED;

        vr_flags |= RF_ANON;

        if (flags & MAP_CONTIG)
            rops = &anon_contig_map_ops;
        else
            rops = &anon_map_ops;

        if (!(vr = mmap_region(mmp, addr, flags, len, vr_flags, rops)))
            return ENOMEM;
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
    struct vir_region* vr;
    off_t offset;
    int retval = 0;

    if (!mmp) return EINVAL;

    if ((retval = map_perm_check(mm_msg.source, who, phys_addr, len)) != 0)
        return retval;

    offset = phys_addr % ARCH_PG_SIZE;
    phys_addr -= offset;
    len += offset;

    len = roundup(len, ARCH_PG_SIZE);

    vr = region_map(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, len,
                    RF_WRITABLE | RF_DIRECT, 0, &direct_phys_map_ops);
    if (!vr) return ENOMEM;

    direct_phys_set_phys(vr, phys_addr);

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
    vir_bytes src_addr = (vir_bytes)mm_msg.u.m_mm_remap.src_addr;
    vir_bytes dest_addr = (vir_bytes)mm_msg.u.m_mm_remap.dest_addr;
    size_t size = mm_msg.u.m_mm_remap.size;

    if (size <= 0) return EINVAL;
    struct mmproc* smmp = endpt_mmproc(src);
    if (!smmp) return EINVAL;
    struct mmproc* dmmp = endpt_mmproc(dest);
    if (!dmmp) return EINVAL;

    struct vir_region* src_region = region_lookup(smmp, src_addr);
    if (src_region->vir_addr != src_addr) return EFAULT;

    if (size % ARCH_PG_SIZE) {
        size = size - (size % ARCH_PG_SIZE) + ARCH_PG_SIZE;
    }

    if (size != src_region->length) return EFAULT;

    int vrflags = RF_SHARED | RF_WRITABLE;
    struct vir_region* new_region = NULL;
    if (dest_addr) {
        new_region =
            region_map(dmmp, dest_addr, 0, size, vrflags, 0, &shared_map_ops);
    } else {
        new_region = region_map(dmmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, size,
                                vrflags, 0, &shared_map_ops);
    }
    if (!new_region) return ENOMEM;

    shared_set_source(new_region, smmp->endpoint, src_region);

    mm_msg.u.m_mm_remap.ret_addr = (void*)new_region->vir_addr;
    return 0;
}

int do_mremap(void)
{
    endpoint_t src = mm_msg.source;
    vir_bytes old_addr = (vir_bytes)mm_msg.u.m_mm_mremap.old_addr;
    size_t old_size = mm_msg.u.m_mm_mremap.old_size;
    size_t new_size = mm_msg.u.m_mm_mremap.new_size;
    int flags = mm_msg.u.m_mm_mremap.flags;
    vir_bytes new_addr = 0, offset;
    struct mmproc* mmp;
    struct vir_region *src_region, *new_region = NULL;
    int retval;

    /* check for invalid flags */
    if (flags & ~(MREMAP_MAYMOVE | MREMAP_FIXED)) return EINVAL;

    /* TODO: handle !MREMAP_MAYMOVE case */
    if (!(flags & MREMAP_MAYMOVE)) return EINVAL;

    /* old_addr was not page-aligned */
    if (old_addr % ARCH_PG_SIZE) return EINVAL;

    if (flags & MREMAP_FIXED) {
        new_addr = (vir_bytes)mm_msg.u.m_mm_mremap.new_addr;
        if (new_addr % ARCH_PG_SIZE) return EINVAL;
    }

    mmp = endpt_mmproc(src);
    if (!mmp) return EINVAL;

    old_size = roundup(old_size, ARCH_PG_SIZE);
    new_size = roundup(new_size, ARCH_PG_SIZE);

    src_region = region_lookup(mmp, old_addr);
    if (!src_region ||
        old_addr + old_size > src_region->vir_addr + src_region->length)
        return EFAULT;

    offset = old_addr - src_region->vir_addr;

    if (flags & MREMAP_FIXED) {
        new_region = region_map(mmp, new_addr, 0, new_size, src_region->flags,
                                0, src_region->rops);
    } else {
        new_region = region_map(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, new_size,
                                src_region->flags, 0, src_region->rops);
    }

    if (!new_region) return ENOMEM;

    retval = region_remap(mmp, src_region, offset, old_size, new_region);
    if (retval != OK) {
        list_del(&new_region->list);
        avl_erase(&new_region->avl, &mmp->mm->mem_avl);
        region_free(new_region);
        return retval;
    }

    mm_msg.u.m_mm_mmap_reply.retaddr = (void*)new_region->vir_addr;
    return 0;
}
