#include <lyos/types.h>
#include <lyos/ipc.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/vm.h>
#include <signal.h>
#include <errno.h>

#include "region.h"
#include "proto.h"

static int direct_phys_pt_flags(const struct vir_region* vr) { return 0; }

static int direct_phys_unreference(struct phys_region* pr) { return 0; }

static int direct_phys_page_fault(struct mmproc* mmp, struct vir_region* vr,
                                  struct phys_region* pr, int write,
                                  vfs_callback_t cb, void* state,
                                  size_t state_len)
{
    phys_bytes paddr;
    paddr = vr->param.phys;

    assert(paddr != PHYS_NONE);
    assert(pr->page->phys_addr == PHYS_NONE);

    paddr += pr->offset;
    assert(paddr != PHYS_NONE);

    pr->page->phys_addr = paddr;
    return 0;
}

static int direct_phys_writable(const struct phys_region* pr)
{
    assert(pr->page->refcount > 0);
    return pr->page->phys_addr != PHYS_NONE;
}

static int direct_phys_copy(struct vir_region* vr, struct vir_region* new_vr)
{
    new_vr->param.phys = vr->param.phys;
    return 0;
}

const struct region_operations direct_phys_map_ops = {
    .rop_pt_flags = direct_phys_pt_flags,
    .rop_copy = direct_phys_copy,
    .rop_page_fault = direct_phys_page_fault,

    .rop_writable = direct_phys_writable,
    .rop_unreference = direct_phys_unreference,
};

void direct_phys_set_phys(struct vir_region* vr, phys_bytes paddr)
{
    vr->param.phys = paddr;
}
