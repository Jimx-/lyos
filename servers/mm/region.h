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

#ifndef _REGION_H_
#define _REGION_H_

#include <lyos/list.h>
#include <lyos/avl.h>

#include "file.h"

/**
 * Physical frame
 */
struct phys_frame {
    phys_bytes phys_addr;

    int refcnt;
    int flags;
};

/**
 * Physical memory region
 */
struct phys_region {
    struct phys_frame ** frames;

    int capacity;
};

/**
 * Virtual memory region
 */
struct vir_region {
    struct list_head list;
    struct avl_node avl;

    struct mmproc * parent;

    vir_bytes vir_addr;
    size_t length;
    struct phys_region phys_block;  /*<- physical memory block */

    int refcnt;
    int flags;

    union {
        struct {
            struct mm_file_desc* filp;
            off_t offset;
            size_t clearend;
        } file;
    } param;
};

#endif /* _REGION_H_ */
