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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/ipc.h>
#include <lyos/fs.h>
#include <lyos/vm.h>
#include <atomic.h>
#include "signal.h"
#include "errno.h"
#include "region.h"
#include "global.h"
#include "proto.h"
#include "const.h"
#include "cache.h"
#include "file.h"

PRIVATE int region_handle_pf_filemap(struct mmproc * mmp, struct vir_region * vr, 
        vir_bytes offset, int wrflag);

/* Page fault state */
struct filemap_pf_state {
    struct mmproc* mmp;
    struct vir_region* vr;
    vir_bytes offset;
    int wrflag;

    phys_bytes cache_phys;
    vir_bytes cache_vir;
};

//#define REGION_DEBUG 1

/**
 * @brief phys_region_free
 * @details Free a physical region.
 * 
 * @param phys_region The physical region to be freed.
 */
PRIVATE void phys_region_free(struct phys_region * rp)
{
    struct phys_frame * frame;
    int i;
    for (i = 0; i < rp->capacity; i++) {
        frame = rp->frames[i];
        if (frame != NULL) {
            if (frame->refcnt) frame->refcnt--;

            if (frame->refcnt <= 0) {
                if (frame->phys_addr) free_mem((int)(frame->phys_addr), ARCH_PG_SIZE);
                SLABFREE(frame);
            }
        }
    }

    free_vmem((int)(rp->frames), rp->capacity * sizeof(struct phys_frame *));
    rp->frames = NULL;
    rp->capacity = 0;
}

PUBLIC int phys_region_init(struct phys_region * rp, int capacity)
{
    int alloc_size = capacity * sizeof(struct phys_frame *);
    if (alloc_size % ARCH_PG_SIZE != 0) {
        alloc_size = (alloc_size / ARCH_PG_SIZE + 1) * ARCH_PG_SIZE;
        capacity = alloc_size / sizeof(struct phys_frame *);
    }

    if (rp->capacity != 0) phys_region_free(rp);

    rp->frames = (struct phys_frame **)alloc_vmem(NULL, alloc_size, 0);
    if (rp->frames == NULL) return ENOMEM;
    memset(rp->frames, 0, alloc_size);
    rp->capacity = capacity;

    return 0;
}

PRIVATE int phys_region_realloc(struct phys_region * rp, int new_capacity)
{
    int alloc_size = new_capacity * sizeof(struct phys_frame *);
    if (alloc_size % ARCH_PG_SIZE != 0) {
        alloc_size = (alloc_size / ARCH_PG_SIZE + 1) * ARCH_PG_SIZE;
        new_capacity = alloc_size / sizeof(struct phys_frame *);
    }

    struct phys_frame * new_frames = (struct phys_frame **)alloc_vmem(NULL, alloc_size, 0);
    if (new_frames == NULL) return ENOMEM;

    memcpy(new_frames, rp->frames, rp->capacity * sizeof(struct phys_frame *));
    free_vmem((int)(rp->frames), rp->capacity * sizeof(struct phys_frame *));
    rp->frames = new_frames;
    rp->capacity = new_capacity;

    return 0;
}

PRIVATE int phys_region_extend_up_to(struct phys_region * rp, int new_capacity)
{
    if (rp->capacity >= new_capacity) return 0;
    else return phys_region_realloc(rp, new_capacity);
}

PRIVATE int phys_region_right_shift(struct phys_region * rp, int new_start)
{
    struct phys_frame ** start = &(rp->frames[new_start]);
    memmove(start, rp->frames, (rp->capacity - new_start) * sizeof(struct phys_frame *));
    memset(rp->frames, 0, new_start * sizeof(struct phys_frame *));

    return 0;
}

PRIVATE struct phys_frame * phys_region_get(struct phys_region * rp, int i)
{
    struct phys_frame * pf = rp->frames[i];
    
    if (pf == NULL) {
        SLABALLOC(pf);
        if (!pf) return NULL;

        memset(pf, 0, sizeof(*pf));
        rp->frames[i] = pf;
    }

    return pf;
}

PRIVATE inline struct phys_frame * phys_region_set(struct phys_region * rp, int i, struct phys_frame * pf)
{
    struct phys_frame* frame = rp->frames[i];

    if (frame != NULL) {
        if (frame->refcnt == 0) SLABFREE(frame);
    }

    rp->frames[i] = pf;

    return pf;
}

/**
 * <Ring 1> Create new virtual memory region.
 * @param  mp         Process the region belongs to.
 * @param  vir_base   Virtual base adress.
 * @param  vir_length Virtual memory length.
 * @return            The memory region created.
 */
PUBLIC struct vir_region * region_new(void * vir_base, int vir_length, int flags)
{
    struct vir_region * region;

    SLABALLOC(region);
    if (!region) return NULL;

#if REGION_DEBUG
    printl("MM: region_new: allocated memory for virtual region at 0x%x\n", (int)region);
#endif

    if (region) {
        region->vir_addr = vir_base;
        region->length = vir_length;
        region->flags = flags;
        region->refcnt = 1;
        struct phys_region * pr = &(region->phys_block);
        pr->capacity = 0;
        if (phys_region_init(pr, region->length / ARCH_PG_SIZE) != 0) return NULL;
    }
    
    return region;
}

/**
 * <Ring 1> Allocate physical memory for rp.
 */
PUBLIC int region_alloc_phys(struct vir_region * rp)
{
    struct phys_region * pregion = &(rp->phys_block);
    int base = (int)rp->vir_addr, len = rp->length;
    int i;

    for (i = 0; len > 0; len -= ARCH_PG_SIZE, base += ARCH_PG_SIZE, i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt > 0 && frame->phys_addr != NULL) continue;
        void * paddr = (void *)alloc_pages(1, APF_NORMAL);
        if (!paddr) return ENOMEM;
        frame->flags = (rp->flags & RF_WRITABLE) ? PFF_WRITABLE : 0;
        frame->phys_addr = paddr; 
        frame->refcnt = 1;
    }
#if REGION_DEBUG
    printl("MM: region_alloc_phys: allocated physical memory for region: v: 0x%x ~ 0x%x(%x bytes)\n", rp->vir_addr, rp->vir_addr + rp->length, rp->length);
#endif

    return 0;
}

/**
 * <Ring 1> Set physical memory for rp.
 */
PUBLIC int region_set_phys(struct vir_region * rp, phys_bytes phys_addr)
{
    struct phys_region * pregion = &(rp->phys_block);
    int len = rp->length;
    int i;

    for (i = 0; len > 0; len -= PG_SIZE, i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt > 0 && frame->phys_addr != NULL) continue;
        frame->flags = (rp->flags & RF_WRITABLE) ? PFF_WRITABLE : 0;
        frame->phys_addr = (void *)phys_addr; 
        frame->refcnt = 1;
        phys_addr += ARCH_PG_SIZE;
    }

    return 0;
}

/**
 * <Ring 1> Map the physical memory of virtual region rp. 
 */
PUBLIC int region_map_phys(struct mmproc * mmp, struct vir_region * rp)
{
#if REGION_DEBUG
    printl("MM: region_map_phys: mapping virtual memory region(0x%x - 0x%x)\n", 
                (int)rp->vir_addr, (int)rp->vir_addr + rp->length);
#endif
    
    struct phys_region * pregion = &(rp->phys_block);
    int base = (int)rp->vir_addr, len = rp->length;
    int i;

    for (i = 0; len > 0; len -= ARCH_PG_SIZE, base += ARCH_PG_SIZE, i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->phys_addr == NULL) continue;
#if REGION_DEBUG
        printl("MM: region_map_phys: mapping page(0x%x -> 0x%x)\n", 
                base, frame->phys_addr);
#endif
        int flags = ARCH_PG_PRESENT | ARCH_PG_USER;
        if (frame->flags & PFF_WRITABLE) flags |= ARCH_PG_RW;
        pt_mappage(&mmp->active_mm->pgd, frame->phys_addr, (void*)base, flags);
        frame->flags |= PFF_MAPPED;
    }

    rp->flags |= RF_MAPPED;

    return 0;
}

PUBLIC struct vir_region * region_find_free_region(struct mmproc * mmp, 
                vir_bytes minv, vir_bytes maxv, vir_bytes len, int flags)
{
    struct vir_region * vr;
    /*int pages = len / ARCH_PG_SIZE;
    if (len % ARCH_PG_SIZE) pages++;

    vir_bytes vaddr = pgd_find_free_pages(&mmp->active_mm->pgd, pages, minv, maxv);
    if (vaddr == 0) return NULL; */
    vir_bytes vaddr = mmp->mmap_base;
    mmp->mmap_base += len;

    if ((vr = region_new(vaddr, len, flags)) == NULL) return NULL;

    return vr;
}

/**
 * <Ring 1> Unmap the physical memory of virtual region rp. 
 */
PUBLIC int region_unmap_phys(struct mmproc * mmp, struct vir_region * rp)
{
    /* not mapped */
    if (!(rp->flags & RF_MAPPED)) return 0;

    unmap_memory(&mmp->active_mm->pgd, rp->vir_addr, rp->length);

    struct phys_region * pregion = &(rp->phys_block);
    int i;

    for (i = 0; i < rp->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        frame->flags &= ~PFF_MAPPED;
    }

    rp->flags &= ~RF_MAPPED;

    return 0;
}

PUBLIC int region_extend_up_to(struct mmproc * mmp, char * addr)
{
    unsigned offset = ~0;
    struct vir_region * vr, * rb = NULL;
    list_for_each_entry(vr, &mmp->active_mm->mem_regions, list) {
        /* need no extend */
        if (addr >= (int)(vr->vir_addr) && addr <= (int)(vr->vir_addr) + vr->length) {
            return 0;
        }

        if (addr < (char *)vr->vir_addr) continue;
        unsigned roff = addr - (char *)vr->vir_addr;
        if (roff < offset) {
            offset = roff;
            rb = vr;
        }
    }

    if (!rb) return EINVAL;

    unsigned increment = addr - (char *)rb->vir_addr - rb->length;
    int retval = 0;
    if ((retval = region_extend(rb, increment)) != 0) return retval;

    region_map_phys(mmp, rb);

    return 0;
}

/**
 * <Ring 1> Extend memory region.
 */
PUBLIC int region_extend(struct vir_region * rp, int increment)
{
    if (increment % PG_SIZE != 0) {
        increment += PG_SIZE - (increment % PG_SIZE);
    }

    rp->length += increment;
    int retval = phys_region_extend_up_to(&(rp->phys_block), rp->length / PG_SIZE);
    if (retval) return retval;
    return region_alloc_phys(rp);
}

PUBLIC int region_share(struct mmproc * p_dest, struct vir_region * dest, 
                            struct mmproc * p_src, struct vir_region * src)
{
    int i;

    dest->vir_addr = src->vir_addr;
    dest->length = src->length;
    dest->flags = src->flags;

    struct phys_region * pregion = &(src->phys_block);
    struct phys_region * prdest = &(dest->phys_block);
    if (prdest->capacity < src->length / ARCH_PG_SIZE)
        phys_region_realloc(prdest, src->length / ARCH_PG_SIZE);
    for (i = 0; i < src->length / PG_SIZE; i++) {
        struct phys_frame * frame = phys_region_get(pregion, i);
        if (frame->refcnt) {
            frame->refcnt++;
            frame->flags |= PFF_SHARED;
            frame->flags &= ~PFF_WRITABLE;
            phys_region_set(prdest, i, frame);
        }
    }

    region_map_phys(p_src, src);

    return 0;
}

PUBLIC struct vir_region * region_lookup(struct mmproc * mmp, vir_bytes addr)
{
    struct vir_region * vr;

    list_for_each_entry(vr, &mmp->active_mm->mem_regions, list) {
        if (addr >= (vir_bytes)(vr->vir_addr) && addr < (vir_bytes)(vr->vir_addr) + vr->length) {
            return vr;
        }
    }

    return NULL;
}

PUBLIC int region_handle_memory(struct mmproc * mmp, struct vir_region * vr, 
        vir_bytes offset, vir_bytes len, int wrflag)
{
    vir_bytes end = offset + len;
    vir_bytes off;
    int retval;

    for (off = offset; off < end; off += ARCH_PG_SIZE) {
        if ((retval = region_handle_pf(mmp, vr, off, wrflag)) != 0) {
           return retval; 
        } 
    }

    return 0;
}

PRIVATE void region_handle_pf_filemap_callback(struct mmproc* mmp, MESSAGE* msg, void* arg)
{
    struct filemap_pf_state* state = (struct filemap_pf_state*) arg;
    struct mmproc* fault_proc = state->mmp;
    struct vir_region* vr = state->vr;

    if (msg->MMRRESULT != 0) goto kill;

    off_t file_offset = vr->param.file.offset + state->offset;
    struct page_cache* cp = find_cache_by_ino(vr->param.file.filp->dev, vr->param.file.filp->ino, file_offset);

    if (!cp) {
        struct phys_frame * frame;
        SLABALLOC(frame);
        if (!frame) goto kill;

        frame->refcnt = 0;
        frame->phys_addr = state->cache_phys;
        frame->flags = PFF_SHARED | (vr->flags & RF_WRITABLE) ? PFF_WRITABLE : 0;

        if (page_cache_add(vr->param.file.filp->dev, 0, vr->param.file.filp->ino, file_offset, state->cache_vir, frame) != 0) goto kill;
    } else {
        free_vmem(state->cache_vir, ARCH_PG_SIZE);
    }

    /* the page is in the cache now, retry */
    int retval = region_handle_pf_filemap(state->mmp, state->vr, state->offset, state->wrflag);
    if (retval == SUSPEND) return;
    if (retval != 0) goto kill;

    if (vmctl(VMCTL_PAGEFAULT_CLEAR, fault_proc->endpoint) != 0) panic("pagefault: vmctl failed");
    return;

kill:
    printl("MM: SIGSEGV %d bad address\n", fault_proc->endpoint);
    if (kernel_kill(fault_proc->endpoint, SIGSEGV) != 0) panic("pagefault: unable to kill proc");
    if (vmctl(VMCTL_PAGEFAULT_CLEAR, fault_proc->endpoint) != 0) panic("pagefault: vmctl failed");
}

PRIVATE int region_handle_pf_filemap(struct mmproc * mmp, struct vir_region * vr, 
        vir_bytes offset, int wrflag)
{
    struct phys_region * pregion = &vr->phys_block;
    struct phys_frame * frame = phys_region_get(pregion, offset / ARCH_PG_SIZE);
    if (!vr->param.file.filp) panic("region_handle_pf_filemap(): BUG: file mapping region has no file param");
    int fd = vr->param.file.filp->fd;
    static char zero_page[ARCH_PG_SIZE];
    static int first = 1;

    if (first) {
        memset((char*) zero_page, 0, ARCH_PG_SIZE);
        first = 0;
    }

    offset = rounddown(offset, PG_SIZE);

    if (frame->phys_addr == NULL) { /* page not present */
        struct page_cache* cp;
        off_t file_offset = vr->param.file.offset + offset;

        cp = find_cache_by_ino(vr->param.file.filp->dev, vr->param.file.filp->ino, file_offset);
        if (cp) {   /* cache hit */
            
            cp->page->refcnt++;
            phys_region_set(pregion, offset / ARCH_PG_SIZE, cp->page);

            if (vr->flags & RF_WRITABLE && vr->flags & RF_PRIVATE) {   /* private mapping */
                region_cow(mmp, vr, offset);
            } else if (wrflag && vr->flags & RF_WRITABLE && !(cp->page->flags & PFF_WRITABLE)) {
                /* want to write, but cache is not writable */
                region_cow(mmp, vr, offset);
            }

            if (vr->flags & RF_WRITABLE && vr->param.file.clearend > 0 && roundup(offset + vr->param.file.clearend, ARCH_PG_SIZE) >= vr->length) {
                frame = phys_region_get(pregion, offset / ARCH_PG_SIZE);
                phys_bytes phaddr = frame->phys_addr;
                phaddr += ARCH_PG_SIZE - vr->param.file.clearend;
                data_copy(NO_TASK, phaddr, SELF, (char*) zero_page, vr->param.file.clearend);
            }

            region_map_phys(mmp, vr);

            return 0;
        }

        /* tell vfs to read in this page */
        phys_bytes buf_phys;
        vir_bytes buf_vir = alloc_vmem(&buf_phys, ARCH_PG_SIZE, 0);
        if (!buf_vir) return ENOMEM;

        memset((void*)buf_vir, 0, ARCH_PG_SIZE);

        struct filemap_pf_state state;
        state.mmp = mmp;
        state.vr = vr;
        state.offset = offset;
        state.wrflag = wrflag;
        state.cache_vir = buf_vir;
        state.cache_phys = buf_phys;

        if (enqueue_vfs_request(mmp, MMR_FDREAD, fd, buf_vir, file_offset, ARCH_PG_SIZE, region_handle_pf_filemap_callback, &state, sizeof(state)) != 0)
            return ENOMEM;

        return SUSPEND;
    }

    return region_cow(mmp, vr, offset);
}

PUBLIC int region_handle_pf(struct mmproc * mmp, struct vir_region * vr, 
        vir_bytes offset, int wrflag)
{
    struct phys_region * pregion = &vr->phys_block;
    struct phys_frame * frame = phys_region_get(pregion, offset / ARCH_PG_SIZE);

    if (vr->flags & RF_FILEMAP && frame->phys_addr == NULL) return region_handle_pf_filemap(mmp, vr, offset, wrflag);

    if (wrflag && frame->flags & PFF_SHARED) {
        /* writing to write-protected page */
        if (frame->refcnt == 1) {
            frame->flags &= ~PFF_SHARED;
            frame->flags |= PFF_WRITABLE;
            region_map_phys(mmp, vr);
            return 0;
        } else {
            return region_cow(mmp, vr, offset);
        }
    }

    /* writable but write-protected, simply remap it */
    if (wrflag && frame->refcnt == 1 && !(frame->flags & PFF_SHARED)) {
        region_map_phys(mmp, vr);
        return 0;
    }
    
    /* page should be present, try remap */
    if (frame->phys_addr && frame->flags & PFF_MAPPED) {
        region_map_phys(mmp, vr);
        return 0;
    }

    struct phys_frame * new_frame;
    SLABALLOC(new_frame);
    if (!new_frame) return ENOMEM;
    memset(new_frame, 0, sizeof(*new_frame));

    void * paddr = (void *)alloc_pages(1, APF_NORMAL);
    if (!paddr) return ENOMEM;
    new_frame->flags = (vr->flags & RF_WRITABLE) ? PFF_WRITABLE : 0;
    new_frame->phys_addr = paddr; 
    new_frame->refcnt = 1;

    phys_region_set(pregion, offset / ARCH_PG_SIZE, new_frame);

    //int retval = region_alloc_phys(vr);
    //if (retval) return retval;

    region_map_phys(mmp, vr);

    return 0;
}

PUBLIC int region_cow(struct mmproc * mmp, struct vir_region * vr, vir_bytes offset)
{
    struct phys_region * pregion = &vr->phys_block;
    struct phys_frame * frame = phys_region_get(pregion, offset / ARCH_PG_SIZE);

    /* release the old frame */
    if (frame->refcnt) frame->refcnt--;
    if (frame->refcnt <= 1) frame->flags |= PFF_WRITABLE;
    
    /* allocate a new frame */
    struct phys_frame * new_frame;
    SLABALLOC(new_frame);
    if (!new_frame) return ENOMEM;
    memset(new_frame, 0, sizeof(*new_frame));

    void * paddr = (void *)alloc_pages(1, APF_NORMAL);
    if (!paddr) return ENOMEM;
    new_frame->phys_addr = paddr; 
    new_frame->refcnt = 1;
    new_frame->flags = (vr->flags & RF_WRITABLE) ? PFF_WRITABLE : 0;
    phys_region_set(pregion, offset / ARCH_PG_SIZE, new_frame);

    region_map_phys(mmp, vr);

    /* copy the data to the new frame */
    return data_copy(mmp->endpoint, (void *)((vir_bytes)vr->vir_addr + offset), NO_TASK, (void *)frame->phys_addr, ARCH_PG_SIZE); 
}

PUBLIC int region_free(struct vir_region * rp)
{
    struct phys_region * pregion = &(rp->phys_block);

    phys_region_free(pregion);
    rp->refcnt--;

    if (rp->flags & RF_FILEMAP) file_unreferenced(rp->param.file.filp);

    if (rp->refcnt <= 0) SLABFREE(rp);

    return 0;
}
