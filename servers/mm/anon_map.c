#include <lyos/types.h>
#include <lyos/ipc.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/vm.h>
#include <errno.h>

#include "region.h"
#include "global.h"
#include "proto.h"

static int anon_pt_flags(const struct vir_region* vr) { return 0; }

static int anon_shrink_low(struct vir_region* vr, vir_bytes len) { return 0; }

static int anon_resize(struct mmproc* mmp, struct vir_region* vr, vir_bytes len)
{
    assert(vr);
    if (vr->length >= len) return 0;

    assert(vr->flags & RF_ANON);
    assert(!(len % ARCH_PG_SIZE));

    vr->length = len;
    return 0;
}

static void anon_split(struct mmproc* mmp, struct vir_region* vr,
                       struct vir_region* r1, struct vir_region* r2)
{}

static int anon_page_fault(struct mmproc* mmp, struct vir_region* vr,
                           struct phys_region* pr, int write, vfs_callback_t cb,
                           void* state, size_t state_len)
{
    static char zero_page[ARCH_PG_SIZE];
    static int first = 1;
    phys_bytes new_paddr;
    int retval;
    assert(pr->page->refcount > 0);

    if (first) {
        memset((char*)zero_page, 0, ARCH_PG_SIZE);
        first = 0;
    }

    new_paddr = alloc_pages(1, 0);
    if (!new_paddr) return ENOMEM;

    if (!(vr->flags & RF_UNINITIALIZED)) {
        if ((retval = data_copy(NO_TASK, (void*)(vir_bytes)new_paddr, SELF,
                                zero_page, ARCH_PG_SIZE)) != OK) {
            free_mem(new_paddr, ARCH_PG_SIZE);
            return retval;
        }
    }

    if (pr->page->phys_addr == PHYS_NONE) {
        pr->page->phys_addr = new_paddr;
        return 0;
    }

    if (pr->page->refcount < 2 || !write) {
        free_mem(new_paddr, ARCH_PG_SIZE);

        return 0;
    }

    return page_cow(vr, pr, new_paddr);
}

static int anon_writable(const struct phys_region* pr)
{
    assert(pr->page->refcount > 0);
    if (pr->page->phys_addr == PHYS_NONE) return 0;
    if (pr->parent->remaps > 0) return 1;

    return pr->page->refcount == 1;
}

static int anon_unreference(struct phys_region* pr)
{
    assert(pr->page->refcount == 0);
    if (pr->page->phys_addr != PHYS_NONE)
        free_mem(pr->page->phys_addr, ARCH_PG_SIZE);
    return 0;
}

static int anon_copy(struct vir_region* vr, struct vir_region* new_vr)
{
    if (!(vr->flags & RF_MAP_SHARED)) return 0;

    new_vr->rops = &shared_map_ops;
    shared_set_source(new_vr, vr->parent->endpoint, vr);

    return 0;
}

const struct region_operations anon_map_ops = {
    .rop_pt_flags = anon_pt_flags,
    .rop_shrink_low = anon_shrink_low,
    .rop_resize = anon_resize,
    .rop_split = anon_split,
    .rop_page_fault = anon_page_fault,
    .rop_copy = anon_copy,

    .rop_writable = anon_writable,
    .rop_unreference = anon_unreference,
};
