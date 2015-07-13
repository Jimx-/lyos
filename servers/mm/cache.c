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

#include "lyos/type.h"
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/vm.h>
#include "region.h"
#include "proto.h"
#include "const.h"
#include "global.h"
#include "cache.h"

#define HASHSIZE    1024

PRIVATE struct list_head cache_hash_ino[HASHSIZE];

PUBLIC void page_cache_init()
{
    int i;
    for (i = 0; i < HASHSIZE; i++) INIT_LIST_HEAD(&cache_hash_ino[i]);

    mem_info.cached = 0;
}

PRIVATE int _hash(int ino)
{
    return ino % HASHSIZE;
}

PRIVATE void cache_add_hash_ino(struct page_cache* cp)
{
    INIT_LIST_HEAD(&cp->hash_ino);
    int hash = _hash(cp->ino);
    list_add(&cp->hash_ino, &cache_hash_ino[hash]);
}

PUBLIC int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset, vir_bytes vir_addr, struct phys_frame* frame)
{
    struct page_cache* cache;
    SLABALLOC(cache);
    if (!cache) return ENOMEM;

    cache->dev = dev;
    cache->dev_offset = dev_offset;
    cache->ino = ino;
    cache->ino_offset = ino_offset;
    cache->vir_addr = vir_addr;
    cache->page = frame;
    cache->page->refcnt++;

    mem_info.cached += ARCH_PG_SIZE;

    cache_add_hash_ino(cache);

    return 0;
}

PUBLIC struct page_cache* find_cache_by_ino(dev_t dev, ino_t ino, off_t ino_off)
{   
    struct page_cache* cp;
    int hash = _hash(ino);
    list_for_each_entry(cp, &cache_hash_ino[hash], hash_ino) {
        if (cp->dev == dev && cp->ino == ino && cp->ino_offset == ino_off) return cp;
    }

    return NULL;
}
