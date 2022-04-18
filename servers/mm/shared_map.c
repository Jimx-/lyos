#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/vm.h>
#include <asm/atomic.h>
#include <signal.h>
#include <errno.h>

#include "region.h"
#include "proto.h"
#include "global.h"

static int shared_pt_flags(const struct vir_region* vr) { return 0; }

static int get_src(struct vir_region* sr, struct mmproc** mmpp,
                   struct vir_region** vrp)
{
    struct mmproc* mmp;
    struct vir_region* vr;

    if (sr->rops != &shared_map_ops) return EINVAL;

    if (sr->param.shared.endpoint == NO_TASK || !sr->param.shared.vaddr)
        return EINVAL;

    mmp = endpt_mmproc(sr->param.shared.endpoint);
    if (!mmp) return EINVAL;

    if (!(vr = region_lookup(mmp, sr->param.shared.vaddr))) return EINVAL;

    if (vr->rops != &anon_map_ops) return EINVAL;
    if (vr->seq != sr->param.shared.seq) return EINVAL;

    *mmpp = mmp;
    *vrp = vr;
    return 0;
}

static int shared_unreference(struct phys_region* pr)
{
    return anon_map_ops.rop_unreference(pr);
}

static void shared_delete(struct vir_region* vr)
{
    struct vir_region* src;
    struct mmproc* mmp;

    if (get_src(vr, &mmp, &src) != OK) return;

    assert(src->remaps);
    src->remaps--;
}

static int shared_page_fault(struct mmproc* mmp, struct vir_region* vr,
                             struct phys_region* pr, int write,
                             void (*cb)(struct mmproc*, MESSAGE*, void*),
                             void* state, size_t state_len)
{
    struct vir_region* src;
    struct mmproc* src_mmp;
    struct phys_region* src_pr;
    int retval;

    if (get_src(vr, &src_mmp, &src) != OK) return EINVAL;

    if (pr->page->phys_addr != PHYS_NONE) return 0;

    page_free(pr->page);

    if (!(src_pr = phys_region_get(src, pr->offset))) {
        if ((retval = region_handle_pf(src_mmp, src, pr->offset, write, NULL,
                                       NULL, 0)) != OK)
            return retval;

        if (!(src_pr = phys_region_get(src, pr->offset)))
            panic("mm: shared_map_page_fault physical region missing after "
                  "source pf");
    }

    page_link(pr, src_pr->page, pr->offset, vr);

    return 0;
}

static int shared_writable(const struct phys_region* pr)
{
    assert(pr->page->refcount);
    return pr->page->phys_addr != PHYS_NONE;
}

static int shared_copy(struct vir_region* vr, struct vir_region* new_vr)
{
    struct vir_region* src;
    struct mmproc* src_mmp;

    if (get_src(vr, &src_mmp, &src) != OK) return EINVAL;

    shared_set_source(new_vr, src_mmp->endpoint, src);

    return 0;
}

const struct region_operations shared_map_ops = {
    .rop_delete = shared_delete,
    .rop_pt_flags = shared_pt_flags,
    .rop_copy = shared_copy,
    .rop_page_fault = shared_page_fault,

    .rop_writable = shared_writable,
    .rop_unreference = shared_unreference,
};

void shared_set_source(struct vir_region* vr, endpoint_t ep,
                       struct vir_region* src)
{
    struct mmproc* mmp;
    struct vir_region* src_region;

    assert(vr->rops == &shared_map_ops);

    if (ep == NO_TASK || !src) return;

    vr->param.shared.endpoint = ep;
    vr->param.shared.vaddr = src->vir_addr;
    vr->param.shared.seq = src->seq;

    if (get_src(vr, &mmp, &src_region) != OK)
        panic("mm: shared_set_source failed to get source region");

    assert(src_region == src);

    src_region->remaps++;
}
