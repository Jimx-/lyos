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

void page_cache_init()
{
    int i;
    for (i = 0; i < HASHSIZE; i++)
        INIT_LIST_HEAD(&cache_hash_ino[i]);

    mem_info.cached = 0;
}

static int _hash(int ino) { return ino % HASHSIZE; }

static void cache_add_hash_ino(struct page_cache* cp)
{
    INIT_LIST_HEAD(&cp->hash_ino);
    int hash = _hash(cp->ino);
    list_add(&cp->hash_ino, &cache_hash_ino[hash]);
}

int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset,
                   struct page* page)
{
    struct page_cache* cache;

    if (page->flags & PFF_INCACHE) return EINVAL;

    SLABALLOC(cache);
    if (!cache) return ENOMEM;

    assert(dev != NO_DEV);

    cache->dev = dev;
    cache->dev_offset = dev_offset;
    cache->ino = ino;
    cache->ino_offset = ino_offset;
    cache->page = page;
    cache->page->refcount++;

    mem_info.cached += ARCH_PG_SIZE;

    cache_add_hash_ino(cache);

    return 0;
}

struct page_cache* find_cache_by_ino(dev_t dev, ino_t ino, off_t ino_off)
{
    struct page_cache* cp;
    int hash = _hash(ino);
    list_for_each_entry(cp, &cache_hash_ino[hash], hash_ino)
    {
        if (cp->dev == dev && cp->ino == ino && cp->ino_offset == ino_off)
            return cp;
    }

    return NULL;
}
