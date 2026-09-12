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

/*
 * Hypervisor guest RAM: zeroed, resident, page-aligned memory objects owned by
 * MM. An object outlives the VMM's own mapping of it: the kernel's backing
 * registration holds a reference until MM is told (through the kernel release
 * queue) that the pages may be freed.
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/hypervisor.h>
#include <string.h>
#include <errno.h>

#include "region.h"
#include "proto.h"
#include "global.h"

#define NR_GUEST_RAM        16
#define GUEST_RAM_MAX_PAGES 16384

struct guest_ram {
    int in_use;
    u32 handle;
    endpoint_t owner;
    unsigned long npages;
    struct page** pages;   /* vmalloc'd */
    struct vir_region* vr; /* the VMM's shared mapping, NULL when unmapped */
    int kern_ref;          /* kernel backing registration active */
};

static struct guest_ram guest_ram_table[NR_GUEST_RAM];
static u32 next_handle = 0x10000;

static struct guest_ram* guest_ram_lookup(u32 handle)
{
    int i;
    for (i = 0; i < NR_GUEST_RAM; i++) {
        if (guest_ram_table[i].in_use && guest_ram_table[i].handle == handle)
            return &guest_ram_table[i];
    }
    return NULL;
}

static void guest_ram_pages_deref(struct guest_ram* obj)
{
    unsigned long i;

    for (i = 0; i < obj->npages; i++) {
        struct page* page = obj->pages[i];
        if (!page) continue;

        assert(page->refcount > 0);
        if (--page->refcount == 0) {
            free_mem(page->phys_addr, ARCH_PG_SIZE);
            SLABFREE(page);
        }
    }
}

static void guest_ram_pages_ref(struct guest_ram* obj)
{
    unsigned long i;
    for (i = 0; i < obj->npages; i++)
        obj->pages[i]->refcount++;
}

static void guest_ram_try_destroy(struct guest_ram* obj)
{
    if (obj->vr) return;
    if (obj->kern_ref) {
        int retval = hv_unregister_backing(obj->handle);

        /* EBUSY: a memslot still owns it. ENOENT: release already queued.
         * Even on success, retain the pages until the release is drained.
         */
        if (retval != OK && retval != EBUSY && retval != ENOENT)
            panic("mm: guest RAM unregister failed: %d", retval);
        return;
    }

    free_vmem(obj->pages, obj->npages * sizeof(struct page*));
    obj->pages = NULL;
    obj->in_use = 0;
}

/*****************************************************************************
 *                        region operations
 *****************************************************************************/

static int guestram_pt_flags(const struct vir_region* vr) { return 0; }

static int guestram_page_fault(struct mmproc* mmp, struct vir_region* vr,
                               struct phys_region* pr, int write,
                               vfs_callback_t cb, void* state, size_t state_len)
{
    /* all pages are resident before the region becomes visible */
    assert(pr->page->phys_addr != PHYS_NONE);
    return OK;
}

static int guestram_writable(const struct phys_region* pr) { return TRUE; }

static int guestram_unreference(struct phys_region* pr)
{
    /* last reference to the page went away */
    free_mem(pr->page->phys_addr, ARCH_PG_SIZE);
    return OK;
}

static void guestram_delete(struct vir_region* vr)
{
    struct guest_ram* obj = guest_ram_lookup(vr->param.guestram.handle);

    if (obj && obj->vr == vr) {
        obj->vr = NULL;
        guest_ram_try_destroy(obj);
    }
}

const struct region_operations guestram_map_ops = {
    .rop_pt_flags = guestram_pt_flags,
    .rop_page_fault = guestram_page_fault,
    .rop_writable = guestram_writable,
    .rop_unreference = guestram_unreference,
    .rop_delete = guestram_delete,
};

/*****************************************************************************
 *                        MM_GUEST_RAM_ALLOC / FREE
 *****************************************************************************/

int do_guest_ram_alloc()
{
    struct mess_mm_guestram* req = &mm_msg.u.m_mm_guestram;
    static char zero_page[ARCH_PG_SIZE];
    static int first = TRUE;
    struct guest_ram* obj = NULL;
    struct mmproc* mmp;
    struct vir_region* vr;
    u64* phys_list;
    size_t length, plist_pages;
    unsigned long i, attached;
    int retval = 0, slot;

    endpoint_t who = req->who == SELF ? mm_msg.source : req->who;
    mmp = endpt_mmproc(who);
    if (!mmp) return EINVAL;

    length = (req->length + ARCH_PG_SIZE - 1) & ~(size_t)(ARCH_PG_SIZE - 1);
    if (length == 0) return EINVAL;
    if (length >> ARCH_PG_SHIFT > GUEST_RAM_MAX_PAGES) return EINVAL;

    if (first) {
        memset(zero_page, 0, sizeof(zero_page));
        first = FALSE;
    }

    for (slot = 0; slot < NR_GUEST_RAM; slot++) {
        if (!guest_ram_table[slot].in_use) {
            obj = &guest_ram_table[slot];
            break;
        }
    }
    if (!obj) return ENOSPC;

    memset(obj, 0, sizeof(*obj));

    obj->npages = length >> ARCH_PG_SHIFT;
    obj->pages = alloc_vmem(NULL, obj->npages * sizeof(struct page*), 0);
    if (!obj->pages) return ENOMEM;
    memset(obj->pages, 0, obj->npages * sizeof(struct page*));

    for (i = 0; i < obj->npages; i++) {
        phys_bytes pa = alloc_pages(1, 0);
        if (!pa) {
            retval = ENOMEM;
            goto free_pages;
        }
        if (data_copy(NO_TASK, (void*)(vir_bytes)pa, SELF, zero_page,
                      ARCH_PG_SIZE) != OK)
            panic("mm: guest_ram zero page failed");
        obj->pages[i] = page_new(pa);
        if (!obj->pages[i]) {
            free_mem(pa, ARCH_PG_SIZE);
            retval = ENOMEM;
            goto free_pages;
        }
    }

    /* map the object into the caller's address space */
    attached = 0;
    vr = region_map(mmp, 0, VM_STACK_TOP, length, RF_READ | RF_WRITE, 0,
                    &guestram_map_ops);
    if (!vr) {
        retval = ENOMEM;
        goto free_pages;
    }

    for (i = 0; i < obj->npages; i++) {
        assert(obj->pages[i]);
        assert(obj->pages[i]->refcount == 0);
        assert(obj->pages[i]->regions.next == &obj->pages[i]->regions);
        assert(obj->pages[i]->regions.prev == &obj->pages[i]->regions);
        if (!page_reference(obj->pages[i], i << ARCH_PG_SHIFT, vr,
                            &guestram_map_ops)) {
            retval = ENOMEM;
            goto free_region;
        }
        attached++;
    }

    if ((retval = region_handle_memory(mmp, vr, 0, length, TRUE, NULL, NULL,
                                       0)) != OK)
        goto free_region;

    /* publish the object before registering with the kernel */
    obj->in_use = TRUE;
    obj->handle = next_handle++;
    if (next_handle < 0x10000) next_handle = 0x10000;
    obj->owner = who;
    obj->vr = vr;
    vr->param.guestram.handle = obj->handle;

    /* register the backing pages with the kernel; it holds a reference */
    plist_pages =
        (obj->npages * sizeof(u64) + ARCH_PG_SIZE - 1) >> ARCH_PG_SHIFT;
    phys_list = alloc_vmem(NULL, plist_pages * ARCH_PG_SIZE, 0);
    if (!phys_list) {
        retval = ENOMEM;
        goto unpublish;
    }
    memset(phys_list, 0, plist_pages * ARCH_PG_SIZE);

    for (i = 0; i < obj->npages; i++)
        phys_list[i] = obj->pages[i]->phys_addr;

    retval = hv_register_backing(obj->handle, obj->npages, phys_list);
    free_vmem(phys_list, plist_pages * ARCH_PG_SIZE);
    if (retval != OK) goto unpublish;

    guest_ram_pages_ref(obj);
    obj->kern_ref = TRUE;

    req->ret_handle = obj->handle;
    req->vaddr = (void*)vr->vir_addr;
    return OK;

unpublish:
    /* pages [0, attached) are freed by the region machinery below; the
     * never-attached remainder (empty at this point) is freed manually */
    obj->in_use = FALSE;
    obj->vr = NULL;

free_region:
    region_unmap_range(mmp, vr->vir_addr, vr->length);
    i = attached;
    goto free_unattached;

free_pages:
    i = 0;
free_unattached:
    for (; i < obj->npages; i++) {
        if (!obj->pages[i]) continue;
        free_mem(obj->pages[i]->phys_addr, ARCH_PG_SIZE);
        SLABFREE(obj->pages[i]);
    }

    free_vmem(obj->pages, obj->npages * sizeof(struct page*));
    obj->pages = NULL;
    return retval;
}

int do_guest_ram_free()
{
    struct mess_mm_guestram* req = &mm_msg.u.m_mm_guestram;
    struct guest_ram* obj = guest_ram_lookup(req->handle);
    struct mmproc* mmp;

    if (!obj) return EINVAL;
    if (mm_msg.source != TASK_MM && mm_msg.source != obj->owner) return EPERM;

    mmp = endpt_mmproc(obj->owner);
    if (mmp && obj->vr) {
        region_unmap_range(mmp, obj->vr->vir_addr, obj->vr->length);
        /* guestram_delete() clears obj->vr and reaps the object */
    } else {
        obj->vr = NULL;
        guest_ram_try_destroy(obj);
    }

    return OK;
}

/*****************************************************************************
 *                        kernel release drain
 *****************************************************************************/

void guest_ram_release_drain()
{
    u32 handle;

    while (1) {
        if (hv_get_release(&handle) != OK) break;
        if (!handle) break;

        struct guest_ram* obj = guest_ram_lookup(handle);
        if (!obj) continue;

        assert(obj->kern_ref);
        obj->kern_ref = FALSE;

        guest_ram_pages_deref(obj);
        guest_ram_try_destroy(obj);
    }
}
