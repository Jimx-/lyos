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
 * Physical frame
 */
struct phys_frame {
    void * phys_addr;

    u8 refcnt;
    u8 flags;
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

    struct mmproc * parent;
    
    void * vir_addr;
    int length;
    struct phys_region phys_block;  /*<- physical memory block */

    int flags;
};

#endif /* _REGION_H_ */
