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

#include "asm/page.h"
#include "region.h"
#include "proto.h"
#include "global.h"
#include "cache.h"

struct file_map_pf_state {
    struct vir_region* vr;
    vir_bytes offset;
    void* buf_vir;
    phys_bytes buf_phys;

    vfs_callback_t real_cb;
    char real_state[40];
    size_t real_state_len;
};

static int file_map_pt_flags(const struct vir_region* vr) { return 0; }

static int file_map_cow(struct mmproc* mmp, struct vir_region* vr,
                        struct phys_region* pr, size_t clearend)
{
    static char zero_page[ARCH_PG_SIZE];
    static int first = 1;
    int retval;

    if (first) {
        memset((char*)zero_page, 0, ARCH_PG_SIZE);
        first = 0;
    }

    if ((retval = page_cow(vr, pr, PHYS_NONE)) != OK) return retval;

    pr->rops = &anon_map_ops;

    if (clearend > 0) {
        assert(clearend < ARCH_PG_SIZE);
        phys_bytes paddr = pr->page->phys_addr;
        phys_bytes offset = ARCH_PG_SIZE - clearend;
        paddr += offset;
        if ((data_copy(NO_TASK, (void*)(vir_bytes)paddr, SELF, zero_page,
                       clearend)) != OK)
            panic("mm: file_map_cow failed to clear page");
    }

    return 0;
}

static void file_map_page_fault_callback(struct mmproc* mmp, MESSAGE* msg,
                                         void* arg)
{
    struct file_map_pf_state* state = arg;
    struct vir_region* vr = state->vr;

    if (msg->MMRRESULT == OK) {
        off_t file_offset = state->offset;
        struct page_cache* cp = find_cache_by_ino(
            vr->param.file.filp->dev, vr->param.file.filp->ino, file_offset);

        free_vmpages(state->buf_vir, 1);

        if (!cp) {
            struct page* page;

            if (!(page = page_new(state->buf_phys))) {
                free_mem(state->buf_phys, ARCH_PG_SIZE);
                goto out;
            }

            page_cache_add(vr->param.file.filp->dev, 0,
                           vr->param.file.filp->ino, file_offset, page);
        } else {
            free_mem(state->buf_phys, ARCH_PG_SIZE);
        }
    } else {
        free_vmem(state->buf_vir, ARCH_PG_SIZE);
    }

out:
    state->real_cb(mmp, msg, state->real_state);
}

static int file_map_page_fault(struct mmproc* mmp, struct vir_region* vr,
                               struct phys_region* pr, int write,
                               vfs_callback_t cb, void* state, size_t state_len)
{
    int fd = vr->param.file.filp->fd;
    int retval;

    assert(pr->page->refcount > 0);
    assert(vr->param.file.inited);
    assert(vr->param.file.filp);
    assert(vr->param.file.filp->dev != NO_DEV);

    if (pr->page->phys_addr == PHYS_NONE) {
        struct page_cache* cp;
        off_t file_offset = vr->param.file.offset + pr->offset;

        cp = find_cache_by_ino(vr->param.file.filp->dev,
                               vr->param.file.filp->ino, file_offset);

        if (cp) {
            retval = 0;
            page_unreference(vr, pr, FALSE);
            page_link(pr, cp->page, pr->offset, vr);

            if (roundup(pr->offset + vr->param.file.clearend, ARCH_PG_SIZE) >=
                vr->length)
                retval = file_map_cow(mmp, vr, pr, vr->param.file.clearend);
            else if (retval == OK && write && !(vr->flags & RF_MAP_SHARED))
                retval = file_map_cow(mmp, vr, pr, 0);

            return retval;
        }

        if (!cb) return EFAULT;

        phys_bytes buf_phys;
        void* buf_vir = alloc_vmem(&buf_phys, ARCH_PG_SIZE, 0);
        if (!buf_vir) return ENOMEM;

        memset((void*)buf_vir, 0, ARCH_PG_SIZE);

        struct file_map_pf_state fm_state;
        fm_state.vr = vr;
        fm_state.offset = file_offset;
        fm_state.buf_vir = buf_vir;
        fm_state.buf_phys = buf_phys;
        fm_state.real_cb = cb;
        memcpy(fm_state.real_state, state, state_len);
        fm_state.real_state_len = state_len;

        if ((enqueue_vfs_request(mmp, MMR_FDREAD, fd, buf_vir, file_offset,
                                 ARCH_PG_SIZE, file_map_page_fault_callback,
                                 &fm_state, sizeof(fm_state))) != OK)
            return ENOMEM;

        return SUSPEND;
    }

    return file_map_cow(mmp, vr, pr, 0);
}

static int file_map_writable(const struct phys_region* pr)
{
    struct vir_region* vr = pr->parent;
    if (vr->flags & RF_MAP_SHARED) return vr->flags & RF_WRITE;

    return 0;
}

static int file_map_unreference(struct phys_region* pr)
{
    assert(pr->page->refcount == 0);
    if (pr->page->phys_addr != PHYS_NONE)
        free_mem(pr->page->phys_addr, ARCH_PG_SIZE);
    return 0;
}

static void file_map_split(struct mmproc* mmp, struct vir_region* vr,
                           struct vir_region* r1, struct vir_region* r2)
{
    assert(!r1->param.file.inited);
    assert(!r2->param.file.inited);
    assert(vr->param.file.inited);
    assert(r1->length + r2->length == vr->length);
    assert(vr->rops == &file_map_ops);
    assert(r1->rops == &file_map_ops);
    assert(r2->rops == &file_map_ops);

    r1->param.file = vr->param.file;
    r2->param.file = vr->param.file;

    file_reference(r1, vr->param.file.filp);
    file_reference(r2, vr->param.file.filp);

    r1->param.file.clearend = 0;
    r2->param.file.offset += r1->length;

    assert(r1->param.file.inited);
    assert(r2->param.file.inited);
}

static int file_map_shrink_low(struct vir_region* vr, vir_bytes len)
{
    assert(vr->param.file.inited);
    vr->param.file.offset += len;
    return 0;
}

static void file_map_delete(struct vir_region* vr)
{
    assert(vr->rops == &file_map_ops);
    assert(vr->param.file.inited);
    assert(vr->param.file.filp);
    file_unreferenced(vr->param.file.filp);
    vr->param.file.filp = NULL;
    vr->param.file.inited = FALSE;
}

static int file_map_copy(struct vir_region* vr, struct vir_region* new_vr)
{
    assert(vr->param.file.inited);
    file_map_set_file(new_vr->parent, new_vr, vr->param.file.filp->fd,
                      vr->param.file.offset, vr->param.file.filp->dev,
                      vr->param.file.filp->ino, vr->param.file.clearend, FALSE,
                      FALSE);
    assert(new_vr->param.file.inited);

    return 0;
}

static int file_map_remap(struct vir_region* vr, vir_bytes offset,
                          vir_bytes len, struct vir_region* new_vr)
{
    assert(new_vr->rops == &file_map_ops);
    assert(vr->param.file.inited);
    file_map_set_file(new_vr->parent, new_vr, vr->param.file.filp->fd,
                      offset + vr->param.file.offset, vr->param.file.filp->dev,
                      vr->param.file.filp->ino, 0, FALSE, FALSE);
    assert(new_vr->param.file.inited);

    return 0;
}

const struct region_operations file_map_ops = {
    .rop_delete = file_map_delete,
    .rop_pt_flags = file_map_pt_flags,
    .rop_shrink_low = file_map_shrink_low,
    .rop_split = file_map_split,
    .rop_copy = file_map_copy,
    .rop_remap = file_map_remap,
    .rop_page_fault = file_map_page_fault,

    .rop_writable = file_map_writable,
    .rop_unreference = file_map_unreference,
};

int file_map_set_file(struct mmproc* mmp, struct vir_region* vr, int fd,
                      loff_t offset, dev_t dev, ino_t ino, size_t clearend,
                      int prefill, int may_close)
{
    struct mm_file_desc* filp;

    filp = get_mm_file_desc(fd, dev, ino);

    assert(filp);
    assert(!vr->param.file.inited);
    assert(dev != NO_DEV);
    file_reference(vr, filp);
    vr->param.file.offset = offset;
    vr->param.file.clearend = clearend;
    vr->param.file.inited = TRUE;

    return 0;
}
