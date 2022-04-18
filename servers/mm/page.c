#include <lyos/types.h>
#include <lyos/ipc.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/vm.h>
#include <errno.h>

#include "region.h"
#include "proto.h"
#include "global.h"

struct page* page_new(phys_bytes phys)
{
    struct page* page;

    SLABALLOC(page);
    if (!page) return NULL;
    memset(page, 0, sizeof(*page));

    if (phys != PHYS_NONE) assert(!(phys % ARCH_PG_SIZE));

    page->phys_addr = phys;
    page->refcount = 0;
    INIT_LIST_HEAD(&page->regions);
    page->flags = 0;

    return page;
}

void page_free(struct page* page)
{
    if (page->phys_addr != PHYS_NONE) free_mem(page->phys_addr, ARCH_PG_SIZE);
    SLABFREE(page);
}

void page_link(struct phys_region* pr, struct page* page, vir_bytes offset,
               struct vir_region* parent)
{
    pr->offset = offset;
    pr->page = page;
    pr->parent = parent;
    list_add(&pr->page_link, &page->regions);
    page->refcount++;
}

struct phys_region* page_reference(struct page* page, vir_bytes offset,
                                   struct vir_region* vr,
                                   const struct region_operations* rops)
{
    struct phys_region* pr;
    SLABALLOC(pr);
    if (!pr) return NULL;
    memset(pr, 0, sizeof(*pr));

    pr->rops = rops;
    page_link(pr, page, offset, vr);

    phys_region_set(vr, offset, pr);

    return pr;
}

void page_unreference(struct vir_region* vr, struct phys_region* pr, int remove)
{
    struct page* page = pr->page;
    int retval;

    assert(page->refcount > 0);
    page->refcount--;

    assert(!list_empty(&pr->page_link));
    list_del(&pr->page_link);

    if (page->refcount == 0) {
        assert(list_empty(&page->regions));

        if ((retval = pr->rops->rop_unreference(pr)) != OK)
            panic("mm: rop_unreference failed");

        SLABFREE(page);
    }

    pr->page = NULL;

    if (remove) phys_region_set(vr, pr->offset, NULL);
}

int page_cow(struct vir_region* vr, struct phys_region* pr, phys_bytes new_page)
{
    struct page* page;

    if (new_page == PHYS_NONE) {
        new_page = alloc_pages(1, 0);
        if (!new_page) return ENOMEM;
    }

    assert(pr->page->phys_addr != PHYS_NONE);

    if (data_copy(NO_TASK, (void*)(vir_bytes)new_page, NO_TASK,
                  (void*)(vir_bytes)pr->page->phys_addr, ARCH_PG_SIZE) != OK)
        panic("mm: page_cow copy page failed");

    if (!(page = page_new(new_page))) {
        free_mem(new_page, ARCH_PG_SIZE);
        return ENOMEM;
    }

    page_unreference(vr, pr, 0);
    page_link(pr, page, pr->offset, vr);

    pr->rops = &anon_map_ops;

    return 0;
}
