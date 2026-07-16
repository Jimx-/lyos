/*
 * This file is part of Lyos.
 *
 * Lyos is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 */

#ifndef _MM_CACHE_H_
#define _MM_CACHE_H_

struct page;

struct page_cache {
    dev_t dev;
    off_t dev_offset;
    ino_t ino;
    off_t ino_offset;
    void* vir_addr;
    struct page* page;
    struct list_head hash_dev;
    struct list_head hash_ino;
};

struct page_cache* find_cache_by_ino(dev_t dev, ino_t ino, off_t ino_off);
struct page_cache* find_cache_by_dev(dev_t dev, off_t dev_off);
int page_cache_add(dev_t dev, off_t dev_offset, ino_t ino, off_t ino_offset,
                   struct page* page);
int do_set_cacheblock(void);
int do_map_cacheblock(void);

#endif
