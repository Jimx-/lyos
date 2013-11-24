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

/**
 * Physical memory region
 */
struct phys_region {
    struct list_head list;

    void * phys_addr;
    void * vir_addr;
    int length;
};

/**
 * Virtual memory region
 */
struct vir_region {
    struct list_head list;

    void * vir_addr;
    int length;
    struct list_head phys_blocks;  /*<- physical memory blocks */

    int flags;
};

#endif /* _REGION_H_ */
