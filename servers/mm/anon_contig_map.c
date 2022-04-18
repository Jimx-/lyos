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

static int anon_contig_pt_flags(const struct vir_region* vr) { return 0; }

static int anon_contig_resize(struct mmproc* mmp, struct vir_region* vr,
                              vir_bytes len)
{
    return ENOMEM;
}

static int anon_contig_new(struct vir_region* vr)
{
    static char zero_page[ARCH_PG_SIZE];
    static int first = 1;
    phys_bytes new_paddr, paddr;
    size_t pages, i;
    int retval;

    if (first) {
        memset((char*)zero_page, 0, ARCH_PG_SIZE);
        first = 0;
    }

    pages = vr->length >> ARCH_PG_SHIFT;
    assert(pages > 0);

    for (i = 0; i < pages; i++) {
        struct page* page = page_new(PHYS_NONE);
        struct phys_region* pr = NULL;
        if (page)
            pr = page_reference(page, i << ARCH_PG_SHIFT, vr,
                                &anon_contig_map_ops);
        if (!pr) {
            if (page) page_free(page);
            region_free(vr);
            return ENOMEM;
        }
    }

    new_paddr = alloc_pages(pages, 0);
    if (!new_paddr) {
        region_free(vr);
        return ENOMEM;
    }

    paddr = new_paddr;

    for (i = 0; i < pages; i++) {
        struct phys_region* pr;
        pr = phys_region_get(vr, i << ARCH_PG_SHIFT);
        assert(pr);
        assert(pr->page);
        assert(pr->page->phys_addr == PHYS_NONE);
        assert(pr->offset == i << ARCH_PG_SHIFT);

        if ((retval = data_copy(NO_TASK, (void*)(vir_bytes)(paddr + pr->offset),
                                SELF, zero_page, ARCH_PG_SIZE)) != OK) {
            panic("mm: anon_contig_new failed to zero page");
        }

        pr->page->phys_addr = paddr + pr->offset;
    }

    return 0;
}

static int anon_contig_page_fault(struct mmproc* mmp, struct vir_region* vr,
                                  struct phys_region* pr, int write,
                                  void (*cb)(struct mmproc*, MESSAGE*, void*),
                                  void* state, size_t state_len)
{
    panic("mm: page fault in anonymous contiguous mapping");
    return EFAULT;
}

static int anon_contig_reference(struct phys_region* pr,
                                 struct phys_region* new_pr)
{
    return ENOMEM;
}

static int anon_contig_unreference(struct phys_region* pr)
{
    return anon_map_ops.rop_unreference(pr);
}

static int anon_contig_writable(const struct phys_region* pr)
{
    return anon_map_ops.rop_writable(pr);
}

static void anon_contig_split(struct mmproc* mmp, struct vir_region* vr,
                              struct vir_region* r1, struct vir_region* r2)
{}

const struct region_operations anon_contig_map_ops = {
    .rop_new = anon_contig_new,
    .rop_pt_flags = anon_contig_pt_flags,
    .rop_resize = anon_contig_resize,
    .rop_split = anon_contig_split,
    .rop_page_fault = anon_contig_page_fault,

    .rop_writable = anon_contig_writable,
    .rop_reference = anon_contig_reference,
    .rop_unreference = anon_contig_unreference,
};
