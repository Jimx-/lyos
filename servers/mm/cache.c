/*
 * This file is part of Lyos.
 *
 * Lyos is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#include <lyos/ipc.h>
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include <lyos/vm.h>
#include "region.h"
#include "proto.h"
#include "global.h"
#include "cache.h"

#define HASHSIZE 1024

static struct list_head cache_hash_ino[HASHSIZE];
static struct list_head cache_hash_dev[HASHSIZE];

static int _hash(u64 key) { return key % HASHSIZE; }

void page_cache_init()
{
    int i;

    for (i = 0; i < HASHSIZE; i++) {
        INIT_LIST_HEAD(&cache_hash_ino[i]);
        INIT_LIST_HEAD(&cache_hash_dev[i]);
    }

    mem_info.cached = 0;
}

static void cache_add_hash_ino(struct page_cache* cp)
{
    INIT_LIST_HEAD(&cp->hash_ino);
    list_add(&cp->hash_ino, &cache_hash_ino[_hash(cp->ino)]);
}

static void cache_add_hash_dev(struct page_cache* cp)
{
    INIT_LIST_HEAD(&cp->hash_dev);
    if (cp->dev_offset < 0) return;
    list_add(&cp->hash_dev, &cache_hash_dev[_hash(cp->dev_offset)]);
}

struct page_cache* find_cache_by_dev(dev_t dev, off_t dev_off)
{
    struct page_cache* cp;

    list_for_each_entry(cp, &cache_hash_dev[_hash(dev_off)], hash_dev)
    {
        if (cp->dev == dev && cp->dev_offset == dev_off) return cp;
    }

    return NULL;
}

struct page_cache* find_cache_by_ino(dev_t dev, ino_t ino, off_t ino_off)
{
    struct page_cache* cp;

    if (ino == VMC_NO_INODE) return NULL;

    list_for_each_entry(cp, &cache_hash_ino[_hash(ino)], hash_ino)
    {
        if (cp->dev == dev && cp->ino == ino && cp->ino_offset == ino_off)
            return cp;
    }

    return NULL;
}

int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset,
                   struct page* page)
{
    struct page_cache* cache;

    assert(dev != NO_DEV);

    if (dev_offset >= 0 && find_cache_by_dev(dev, dev_offset)) return OK;
    if (ino != VMC_NO_INODE && find_cache_by_ino(dev, ino, ino_offset))
        return OK;

    if (page->flags & PFF_INCACHE) return EINVAL;

    SLABALLOC(cache);
    if (!cache) return ENOMEM;

    cache->dev = dev;
    cache->dev_offset = dev_offset;
    cache->ino = ino;
    cache->ino_offset = ino_offset;
    cache->page = page;
    cache->page->refcount++;
    cache->page->flags |= PFF_INCACHE;

    mem_info.cached += ARCH_PG_SIZE;

    cache_add_hash_ino(cache);
    cache_add_hash_dev(cache);

    return OK;
}

static struct page_cache* find_cacheblock(dev_t dev, off_t dev_offset,
                                          ino_t ino, off_t ino_offset)
{
    struct page_cache* cp;

    if (ino != VMC_NO_INODE) {
        cp = find_cache_by_ino(dev, ino, ino_offset);
        if (cp) return cp;
    }

    return find_cache_by_dev(dev, dev_offset);
}

static int cacheblock_pt_flags(const struct vir_region* vr) { return 0; }

static int cacheblock_page_fault(struct mmproc* mmp, struct vir_region* vr,
                                 struct phys_region* pr, int write,
                                 vfs_callback_t cb, void* state,
                                 size_t state_len)
{
    return OK;
}

static int cacheblock_writable(const struct phys_region* pr)
{
    return pr->page->phys_addr != PHYS_NONE;
}

static int cacheblock_unreference(struct phys_region* pr)
{
    if (pr->page->phys_addr != PHYS_NONE && !(pr->page->flags & PFF_INCACHE))
        free_mem(pr->page->phys_addr, ARCH_PG_SIZE);

    return OK;
}

const struct region_operations cacheblock_map_ops = {
    .rop_pt_flags = cacheblock_pt_flags,
    .rop_page_fault = cacheblock_page_fault,
    .rop_writable = cacheblock_writable,
    .rop_unreference = cacheblock_unreference,
};

int do_set_cacheblock(void)
{
    struct mess_mm_cacheblock* req = &mm_msg.u.m_mm_cacheblock;
    endpoint_t who = req->who == SELF ? mm_msg.source : req->who;
    struct mmproc* mmp = endpt_mmproc(who);
    struct vir_region* vr;
    struct phys_region* pr;
    vir_bytes vaddr = (vir_bytes)req->vaddr;
    off_t offset;
    int retval;

    if (!mmp || req->dev == NO_DEV || req->len != ARCH_PG_SIZE)
        return EINVAL;
    if (vaddr % ARCH_PG_SIZE) return EINVAL;

    vr = region_lookup(mmp, vaddr);
    if (!vr) return EINVAL;

    offset = vaddr - vr->vir_addr;
    if (offset >= vr->length) return EINVAL;

    retval = region_handle_pf(mmp, vr, offset, FALSE, NULL, NULL, 0);
    if (retval != OK) return retval;

    pr = phys_region_get(vr, offset);
    if (!pr || !pr->page || pr->page->phys_addr == PHYS_NONE) return EINVAL;

    return page_cache_add(req->dev, req->dev_offset, req->ino, req->ino_offset,
                          pr->page);
}

int do_map_cacheblock(void)
{
    struct mess_mm_cacheblock* req = &mm_msg.u.m_mm_cacheblock;
    endpoint_t who = req->who == SELF ? mm_msg.source : req->who;
    struct mmproc* mmp = endpt_mmproc(who);
    struct page_cache* cp;
    struct vir_region* vr;
    int retval;

    if (!mmp || req->dev == NO_DEV || req->len != ARCH_PG_SIZE)
        return EINVAL;

    cp = find_cacheblock(req->dev, req->dev_offset, req->ino, req->ino_offset);
    if (!cp) return ENOENT;

    vr = region_map(mmp, ARCH_BIG_PAGE_SIZE, VM_STACK_TOP, ARCH_PG_SIZE,
                    RF_READ | RF_WRITE, 0, &cacheblock_map_ops);
    if (!vr) return ENOMEM;

    if (!page_reference(cp->page, 0, vr, &cacheblock_map_ops)) {
        region_free(vr);
        return ENOMEM;
    }

    retval = region_handle_pf(mmp, vr, 0, FALSE, NULL, NULL, 0);
    if (retval != OK) {
        region_free(vr);
        return retval;
    }

    mm_msg.ADDR = (void*)vr->vir_addr;
    return OK;
}
