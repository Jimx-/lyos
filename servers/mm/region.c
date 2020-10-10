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
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/fs.h>
#include <lyos/vm.h>
#include <asm/atomic.h>
#include <signal.h>
#include <errno.h>

#include "region.h"
#include "global.h"
#include "proto.h"
#include "const.h"
#include "cache.h"
#include "file.h"

#define REGION_DEBUG 0
#if REGION_DEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

#define FREE_REGION_FAILED ((vir_bytes)-1)

static inline size_t phys_slot(vir_bytes offset)
{
    assert(!(offset % ARCH_PG_SIZE));
    return offset >> ARCH_PG_SHIFT;
}

static inline size_t phys_regions_size(struct vir_region* vr)
{
    assert(!(vr->length % ARCH_PG_SIZE));
    return phys_slot(vr->length) * sizeof(struct phys_region*);
}

static inline int phys_region_writable(struct vir_region* vr,
                                       struct phys_region* pr)
{
    assert(pr->rops->rop_writable);
    return (vr->flags & RF_WRITABLE) && (pr->rops->rop_writable(pr));
}

struct phys_region* phys_region_get(struct vir_region* vr, vir_bytes offset)
{
    size_t i;
    struct phys_region* pr;

    assert(offset < vr->length);
    assert(vr->phys_regions);
    assert(!(offset % ARCH_PG_SIZE));

    i = phys_slot(offset);
    if ((pr = vr->phys_regions[i]) != NULL) assert(pr->offset == offset);
    return pr;
}

void phys_region_set(struct vir_region* vr, vir_bytes offset,
                     struct phys_region* pr)
{
    size_t i;
    struct mm_struct* mm;

    assert(offset < vr->length);
    assert(vr->phys_regions);
    assert(!(offset % ARCH_PG_SIZE));

    i = phys_slot(offset);

    mm = vr->mm;
    assert(mm);

    if (pr) {
        assert(!vr->phys_regions[i]);
        assert(pr->offset == offset);
        mm->vm_total += ARCH_PG_SIZE;
    } else {
        assert(vr->phys_regions[i]);
        mm->vm_total -= ARCH_PG_SIZE;
    }

    vr->phys_regions[i] = pr;
}

int phys_region_resize(struct vir_region* vr, vir_bytes new_length)
{
    size_t cur_slots, new_slots;
    struct phys_region** new_prs;

    assert(vr->length);
    assert(new_length);

    cur_slots = phys_slot(vr->length);
    new_slots = phys_slot(new_length);

    new_prs = alloc_vmem(NULL, new_slots * sizeof(struct phys_region*), 0);
    if (!new_prs) return ENOMEM;

    memcpy(new_prs, vr->phys_regions,
           min(cur_slots, new_slots) * sizeof(struct phys_region*));
    if (new_slots > cur_slots)
        memset(new_prs + cur_slots, 0,
               (new_slots - cur_slots) * sizeof(struct phys_region*));

    free_vmem(vr->phys_regions, cur_slots * sizeof(struct phys_region*));
    vr->phys_regions = new_prs;

    return 0;
}

/**
 * <Ring 3> Create a new virtual memory region.
 * @param  mp         Process the region belongs to.
 * @param  vir_base   Virtual base adress.
 * @param  vir_length Virtual memory length.
 * @return            The memory region created.
 */
struct vir_region* region_new(struct mmproc* mmp, vir_bytes base, size_t length,
                              int flags, const struct region_operations* rops)
{
    struct vir_region* region;
    struct phys_region** prs;

    SLABALLOC(region);
    if (!region) return NULL;

    DBG(printl("MM: region_new: allocated memory for virtual region at %p\n",
               region));

    memset(region, 0, sizeof(*region));
    region->vir_addr = base;
    region->length = length;
    region->flags = flags;
    region->rops = rops;
    region->parent = mmp;
    region->mm = mmp->mm;

    if ((prs = alloc_vmem(NULL, phys_regions_size(region), 0)) == NULL) {
        SLABFREE(region);
        return NULL;
    }

    memset(prs, 0, phys_regions_size(region));
    region->phys_regions = prs;

    return region;
}

struct vir_region* region_map(struct mmproc* mmp, vir_bytes minv,
                              vir_bytes maxv, vir_bytes length, int flags,
                              int map_flags,
                              const struct region_operations* rops)
{
    vir_bytes startv;
    struct vir_region* vr;

    assert(mmp->mm);

    startv = region_find_free_region(mmp, minv, maxv, length);
    if (startv == FREE_REGION_FAILED) return NULL;

    if ((vr = region_new(mmp, startv, length, flags, rops)) == NULL)
        return NULL;

    if (vr->rops->rop_new) {
        if (vr->rops->rop_new(vr) != OK) return NULL;
    }

    if (map_flags & MRF_PREALLOC) {
        if (region_handle_memory(mmp, vr, 0, length, TRUE, NULL, NULL, 0) !=
            OK) {
            region_free(vr);
            return NULL;
        }
    }

    vr->flags &= ~RF_UNINITIALIZED;

    list_add(&vr->list, &mmp->mm->mem_regions);
    avl_insert(&vr->avl, &mmp->mm->mem_avl);

    return vr;
}

int region_write_map_page(struct mmproc* mmp, struct vir_region* vr,
                          struct phys_region* pr)
{
    int flags = ARCH_PG_PRESENT | ARCH_PG_USER;
    struct page* page = pr->page;

    assert(vr);
    assert(pr);
    assert(page);
    assert(vr->rops);

    assert(!(vr->vir_addr % ARCH_PG_SIZE));
    assert(!(pr->offset % ARCH_PG_SIZE));
    assert(page->refcount);

    if (phys_region_writable(vr, pr)) flags |= ARCH_PG_RW;

    if (vr->rops->rop_pt_flags) flags |= vr->rops->rop_pt_flags(vr);

    if (pt_writemap(&mmp->mm->pgd, page->phys_addr, vr->vir_addr + pr->offset,
                    ARCH_PG_SIZE, flags) != OK)
        return ENOMEM;

    return 0;
}

int region_write_map(struct mmproc* mmp)
{
    struct vir_region* vr;
    struct phys_region* pr;
    int retval;

    assert(mmp->mm);

    list_for_each_entry(vr, &mmp->mm->mem_regions, list)
    {
        vir_bytes off;

        for (off = 0; off < vr->length; off += ARCH_PG_SIZE) {
            if (!(pr = phys_region_get(vr, off))) continue;

            if ((retval = region_write_map_page(mmp, vr, pr)) != OK)
                return retval;
        }
    }

    return 0;
}

vir_bytes region_find_free_region(struct mmproc* mmp, vir_bytes minv,
                                  vir_bytes maxv, size_t len)
{
    int found = 0;
    vir_bytes vaddr;

    if (!maxv) maxv = minv + len;
    if (minv + len > maxv) return FREE_REGION_FAILED;

    struct avl_iter iter;
    struct vir_region vr_max;
    vr_max.vir_addr = maxv;
    region_avl_start_iter(&mmp->mm->mem_avl, &iter, &vr_max, AVL_GREATER_EQUAL);
    struct vir_region* last = region_avl_get_iter(&iter);

#define TRY_ALLOC_REGION(start, end)                          \
    do {                                                      \
        vir_bytes rstart = ((start) > minv) ? (start) : minv; \
        vir_bytes rend = ((end) < maxv) ? (end) : maxv;       \
        if (rend > rstart && (rend - rstart >= len)) {        \
            vaddr = rend - len;                               \
            found = 1;                                        \
        }                                                     \
    } while (0)

#define ALLOC_REGION(start, end)                                      \
    do {                                                              \
        TRY_ALLOC_REGION((start) + ARCH_PG_SIZE, (end)-ARCH_PG_SIZE); \
        if (!found) {                                                 \
            TRY_ALLOC_REGION(start, end);                             \
        }                                                             \
    } while (0)

    if (!last) {
        region_avl_start_iter(&mmp->mm->mem_avl, &iter, &vr_max, AVL_LESS);
        last = region_avl_get_iter(&iter);
        ALLOC_REGION(last ? (last->vir_addr + last->length) : 0, VM_STACK_TOP);
    }

    if (!found) {
        struct vir_region* vr;
        while ((vr = region_avl_get_iter(&iter)) && !found) {
            struct vir_region* nextvr;
            region_avl_dec_iter(&iter);
            nextvr = region_avl_get_iter(&iter);
            ALLOC_REGION(nextvr ? nextvr->vir_addr + nextvr->length : 0,
                         vr->vir_addr);
        }
    }

    if (!found) {
        return FREE_REGION_FAILED;
    }

    return vaddr;
}

int region_extend_up_to(struct mmproc* mmp, vir_bytes addr)
{
    unsigned offset = ~0;
    struct vir_region *vr, *rb = NULL;
    vir_bytes limit, extra;
    int retval;

    addr = roundup(addr, ARCH_PG_SIZE);

    list_for_each_entry(vr, &mmp->mm->mem_regions, list)
    {
        /* need no extend */
        if (addr >= vr->vir_addr && addr <= vr->vir_addr + vr->length) {
            return 0;
        }

        if (addr < vr->vir_addr) continue;
        unsigned roff = addr - vr->vir_addr;
        if (roff < offset) {
            offset = roff;
            rb = vr;
        }
    }

    if (!rb) return EINVAL;

    limit = rb->vir_addr + rb->length;
    extra = addr - limit;

    if (!rb->rops->rop_resize) {
        if (!region_map(mmp, limit, 0, extra, RF_WRITABLE | RF_ANON, 0,
                        &anon_map_ops))
            return ENOMEM;
        return 0;
    }

    if ((retval = phys_region_resize(rb, addr - rb->vir_addr)) != OK)
        return retval;

    return rb->rops->rop_resize(mmp, rb, addr - rb->vir_addr);
}

int region_handle_memory(struct mmproc* mmp, struct vir_region* vr,
                         off_t offset, size_t len, int write, vfs_callback_t cb,
                         void* state, size_t state_len)
{
    off_t end = offset + len;
    off_t off;
    int retval;

    assert(len > 0);
    assert(end > offset);

    for (off = offset; off < end; off += ARCH_PG_SIZE) {
        if ((retval = region_handle_pf(mmp, vr, off, write, cb, state,
                                       state_len)) != OK)
            return retval;
    }

    return 0;
}

int region_handle_pf(struct mmproc* mmp, struct vir_region* vr,
                     vir_bytes offset, int write, vfs_callback_t cb,
                     void* state, size_t state_len)
{

    struct phys_region* pr;
    int retval = 0;

    offset = rounddown(offset, ARCH_PG_SIZE);

    assert(offset < vr->length);
    assert(!(vr->vir_addr % ARCH_PG_SIZE));
    assert(!(write && !(vr->flags & RF_WRITABLE)));

    if (!(pr = phys_region_get(vr, offset))) {
        struct page* page;

        if (!(page = page_new(PHYS_NONE))) return ENOMEM;

        if (!(pr = page_reference(page, offset, vr, vr->rops))) {
            page_free(page);
            return ENOMEM;
        }
    }

    assert(pr);
    assert(pr->page);
    assert(pr->rops->rop_writable);

    if (!write || !pr->rops->rop_writable(pr)) {
        assert(pr->rops->rop_page_fault);

        if ((retval = pr->rops->rop_page_fault(mmp, vr, pr, write, cb, state,
                                               state_len)) == SUSPEND)
            return SUSPEND;

        if (retval != OK) {
            if (pr) page_unreference(vr, pr, TRUE);
            return retval;
        }

        assert(pr->page);
        assert(pr->page->phys_addr != PHYS_NONE);
    }

    assert(pr->page);
    assert(pr->page->phys_addr != PHYS_NONE);

    if ((retval = region_write_map_page(mmp, vr, pr)) != OK) return retval;

    return 0;
}

static int region_split(struct mmproc* mmp, struct vir_region* vr, size_t len,
                        struct vir_region** v1, struct vir_region** v2)
{
    struct vir_region *vr1 = NULL, *vr2 = NULL;
    size_t rem_len = vr->length - len;
    size_t slot1, slot2;
    off_t off;

    if (!vr->rops->rop_split) return EINVAL;

    assert(!(len % ARCH_PG_SIZE));
    assert(!(rem_len % ARCH_PG_SIZE));
    assert(!(vr->vir_addr % ARCH_PG_SIZE));
    assert(!(vr->length % ARCH_PG_SIZE));

    slot1 = phys_slot(len);
    slot2 = phys_slot(rem_len);
    assert(slot1 + slot2 == phys_slot(vr->length));

    if (!(vr1 = region_new(mmp, vr->vir_addr, len, vr->flags, vr->rops)))
        goto failed;
    if (!(vr2 = region_new(mmp, vr->vir_addr + len, rem_len, vr->flags,
                           vr->rops)))
        goto failed;

    for (off = 0; off < vr1->length; off += ARCH_PG_SIZE) {
        struct phys_region* pr;
        if (!(pr = phys_region_get(vr, off))) continue;
        if (!page_reference(pr->page, off, vr1, pr->rops)) goto failed;
    }

    for (off = 0; off < vr2->length; off += ARCH_PG_SIZE) {
        struct phys_region* pr;
        if (!(pr = phys_region_get(vr, len + off))) continue;
        if (!page_reference(pr->page, off, vr2, pr->rops)) goto failed;
    }

    vr->rops->rop_split(mmp, vr, vr1, vr2);

    list_del(&vr->list);
    avl_erase(&vr->avl, &mmp->mm->mem_avl);
    region_free(vr);

    list_add(&vr1->list, &mmp->mm->mem_regions);
    avl_insert(&vr1->avl, &mmp->mm->mem_avl);
    list_add(&vr2->list, &mmp->mm->mem_regions);
    avl_insert(&vr2->avl, &mmp->mm->mem_avl);

    *v1 = vr1;
    *v2 = vr2;

    return 0;

failed:
    if (vr1) region_free(vr1);
    if (vr2) region_free(vr2);

    return ENOMEM;
}

struct vir_region* region_copy_region(struct mmproc* mmp, struct vir_region* vr)
{
    struct vir_region* new_vr;
    struct phys_region *pr, *new_pr;
    size_t i;
    int retval;

    if ((new_vr = region_new(vr->parent, vr->vir_addr, vr->length, vr->flags,
                             vr->rops)) == NULL)
        return NULL;

    new_vr->parent = mmp;

    if (vr->rops->rop_copy && (retval = vr->rops->rop_copy(vr, new_vr)) != OK) {
        region_free(new_vr);
        return NULL;
    }

    for (i = 0; i < phys_slot(vr->length); i++) {
        if (!(pr = phys_region_get(vr, i << ARCH_PG_SHIFT))) continue;

        if (!(new_pr =
                  page_reference(pr->page, pr->offset, new_vr, vr->rops))) {
            region_free(new_vr);
            return NULL;
        }

        if (pr->rops->rop_reference) pr->rops->rop_reference(pr, new_pr);
    }

    return new_vr;
}

int region_copy_proc(struct mmproc* mmp_dest, struct mmproc* mmp_src)
{
    struct vir_region *vr, *new_vr;

    assert(mmp_src->mm);
    assert(mmp_dest->mm);

    list_for_each_entry(vr, &mmp_src->mm->mem_regions, list)
    {
        if (!(new_vr = region_copy_region(mmp_dest, vr))) {
            region_free_mm(mmp_dest->mm);
            return ENOMEM;
        }

        assert(new_vr->length == vr->length);
        list_add(&new_vr->list, &mmp_dest->mm->mem_regions);
        avl_insert(&new_vr->avl, &mmp_dest->mm->mem_avl);
    }

    region_write_map(mmp_src);
    region_write_map(mmp_dest);

    return 0;
}

static int region_subfree(struct vir_region* rp, off_t start, size_t len)
{
    struct phys_region* pr;
    vir_bytes end = start + len;
    vir_bytes offset;

    assert(!(start % ARCH_PG_SIZE));

    for (offset = start; offset < end; offset += ARCH_PG_SIZE) {
        if ((pr = phys_region_get(rp, offset)) == NULL) continue;

        assert(pr->offset >= start);
        assert(pr->offset < end);
        page_unreference(rp, pr, TRUE /* remove */);
        SLABFREE(pr);
    }

    return 0;
}

static int region_unmap(struct mmproc* mmp, struct vir_region* vr, off_t offset,
                        size_t len)
{
    int retval;
    size_t new_slots;
    struct phys_region* pr;
    struct phys_region** new_prs;
    size_t free_slots = phys_slot(len);
    vir_bytes unmap_start;
    off_t voff;

    assert(offset + len <= vr->length);
    assert(!(len % ARCH_PG_SIZE));

    region_subfree(vr, offset, len);

    unmap_start = vr->vir_addr + offset;

    if (len == vr->length) {
        list_del(&vr->list);
        avl_erase(&vr->avl, &mmp->mm->mem_avl);
        region_free(vr);
    } else if (offset == 0) {
        if (!vr->rops->rop_shrink_low) return EINVAL;

        if ((retval = vr->rops->rop_shrink_low(vr, len)) != 0) return EINVAL;

        list_del(&vr->list);
        avl_erase(&vr->avl, &mmp->mm->mem_avl);

        vr->vir_addr += len;

        assert(vr->length > len);
        new_slots = phys_slot(vr->length - len);
        assert(new_slots);

        list_add(&vr->list, &mmp->mm->mem_regions);
        avl_insert(&vr->avl, &mmp->mm->mem_avl);

        for (voff = len; voff < vr->length; voff += ARCH_PG_SIZE) {
            if (!(pr = phys_region_get(vr, voff))) continue;
            assert(pr->offset >= offset);
            assert(pr->offset >= len);
            pr->offset -= len;
        }

        new_prs = alloc_vmem(NULL, new_slots * sizeof(struct phys_region*), 0);
        memcpy(new_prs, vr->phys_regions + free_slots,
               new_slots * sizeof(struct phys_region*));
        free_vmem(vr->phys_regions, phys_regions_size(vr));
        vr->phys_regions = new_prs;

        vr->length -= len;
    } else if (offset + len == vr->length) {
        if ((retval = phys_region_resize(vr, vr->length - len)) != OK)
            return retval;

        vr->length -= len;
    }

    unmap_memory(&mmp->mm->pgd, unmap_start, len);

    return 0;
}

int region_unmap_range(struct mmproc* mmp, vir_bytes start, size_t len)
{
    off_t offset = start % ARCH_PG_SIZE;
    struct vir_region *vr, *next_vr;
    struct avl_iter iter;
    struct vir_region vr_start;
    vir_bytes limit;

    start -= offset;
    len += offset;
    len = roundup(len, ARCH_PG_SIZE);
    limit = start + len;

    vr_start.vir_addr = start;
    region_avl_start_iter(&mmp->mm->mem_avl, &iter, &vr_start, AVL_LESS_EQUAL);
    if (!(vr = region_avl_get_iter(&iter))) {
        region_avl_start_iter(&mmp->mm->mem_avl, &iter, &vr_start, AVL_GREATER);
        if (!(vr = region_avl_get_iter(&iter))) {
            return 0;
        }
    }

    assert(vr);

    for (; vr && vr->vir_addr < limit; vr = next_vr) {
        region_avl_inc_iter(&iter);
        next_vr = region_avl_get_iter(&iter);

        vir_bytes cur_start = max(start, vr->vir_addr);
        vir_bytes cur_limit = min(limit, vr->vir_addr + vr->length);
        if (cur_start >= cur_limit) continue;

        /* need spliting */
        if (cur_start > vr->vir_addr && cur_limit < vr->vir_addr + vr->length) {
            struct vir_region *v1, *v2;
            size_t split_len = cur_limit - vr->vir_addr;

            assert(split_len > 0);
            assert(split_len < vr->length);

            int retval;
            if ((retval = region_split(mmp, vr, split_len, &v1, &v2))) {
                return retval;
            }
            vr = v1;
        }

        assert(cur_start >= vr->vir_addr);
        assert(cur_limit > vr->vir_addr);
        assert(cur_limit <= vr->vir_addr + vr->length);

        int retval = region_unmap(mmp, vr, cur_start - vr->vir_addr,
                                  cur_limit - cur_start);

        if (retval) return retval;

        if (next_vr) {
            region_avl_start_iter(&mmp->mm->mem_avl, &iter, next_vr, AVL_EQUAL);
            assert(region_avl_get_iter(&iter) == next_vr);
        }
    }

    return 0;
}

int region_free(struct vir_region* rp)
{
    int retval;

    assert(rp->phys_regions);

    if ((retval = region_subfree(rp, 0, rp->length)) != OK) return retval;

    if (rp->rops->rop_delete) rp->rops->rop_delete(rp);
    free_vmem(rp->phys_regions, phys_regions_size(rp));
    rp->phys_regions = NULL;
    SLABFREE(rp);

    return 0;
}

int region_free_mm(struct mm_struct* mm)
{
    struct vir_region *vr, *tmp;

    list_for_each_entry_safe(vr, tmp, &mm->mem_regions, list)
    {
        list_del(&vr->list);
        region_free(vr);
    }

    INIT_LIST_HEAD(&mm->mem_regions);
    region_init_avl(mm);

    return 0;
}
