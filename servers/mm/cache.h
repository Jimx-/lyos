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

#ifndef _MM_CACHE_H_
#define _MM_CACHE_H_

struct page_cache {
    dev_t dev;
    off_t dev_offset;

    ino_t ino;
    off_t ino_offset;

    void* vir_addr;
    struct phys_frame* page;

    struct list_head hash_dev;
    struct list_head hash_ino;
};

PUBLIC struct page_cache* find_cache_by_ino(dev_t dev, ino_t ino, off_t ino_off);

#endif
