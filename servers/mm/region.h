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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/list.h>
#include <lyos/avl.h>
#include <sys/mman.h>

#include "file.h"

/**
 * Physical page frame
 */
struct page {
    phys_bytes phys_addr;
    struct list_head regions; /* physical regions that reference this page */
    u16 refcount;
    u16 flags;
};

/* Physical page fram flags */
#define PFF_INCACHE 0x1

struct phys_region;
struct vir_region;
struct mmproc;

struct region_operations {
    int (*rop_new)(struct vir_region* vr);
    void (*rop_delete)(struct vir_region* vr);
    int (*rop_pt_flags)(const struct vir_region* vr);
    int (*rop_shrink_low)(struct vir_region* vr, vir_bytes len);
    int (*rop_resize)(struct mmproc* mmp, struct vir_region* vr, vir_bytes len);
    void (*rop_split)(struct mmproc* mmp, struct vir_region* vr,
                      struct vir_region* r1, struct vir_region* r2);
    int (*rop_copy)(struct vir_region* vr, struct vir_region* new_vr);
    int (*rop_remap)(struct vir_region* vr, vir_bytes offset, vir_bytes len,
                     struct vir_region* new_vr);
    int (*rop_page_fault)(struct mmproc* mmp, struct vir_region* vr,
                          struct phys_region* pr, int write,
                          void (*cb)(struct mmproc*, MESSAGE*, void*),
                          void* state, size_t state_len);

    int (*rop_writable)(const struct phys_region* pr);
    int (*rop_reference)(struct phys_region* pr, struct phys_region* new_pr);
    int (*rop_unreference)(struct phys_region* pr);
};

/**
 * Physical memory region
 */
struct phys_region {
    struct page* page;
    struct vir_region* parent;
    vir_bytes offset;

    struct list_head page_link;
    const struct region_operations* rops;
};

/**
 * Virtual memory region
 */
struct vir_region {
    struct list_head list;
    struct avl_node avl;

    struct mmproc* parent;
    struct mm_struct* mm;

    vir_bytes vir_addr;
    size_t length;
    int flags;
    int remaps;
    int seq;

    struct phys_region** phys_regions;
    const struct region_operations* rops;

    union {
        struct {
            struct mm_file_desc* filp;
            off_t offset;
            size_t clearend;
            int inited;
        } file;

        struct {
            endpoint_t endpoint;
            vir_bytes vaddr;
            int seq;
        } shared;

        phys_bytes phys;
    } param;
};

/* Region flags */
#define RF_READ          0x0001
#define RF_WRITE         0x0002
#define RF_EXEC          0x0004
#define RF_SHARED        0x0008
#define RF_UNINITIALIZED 0x0010
#define RF_MAP_SHARED    0x0020
#define RF_ANON          0x0100
#define RF_DIRECT        0x0200
#define RF_IO            0x0400

/* Map region flags */
#define MRF_PREALLOC 0x01

static inline int region_get_prot_bits(int prot)
{
    return ((prot & PROT_READ) ? RF_READ : 0) |
           ((prot & PROT_WRITE) ? RF_WRITE : 0) |
           ((prot & PROT_EXEC) ? RF_EXEC : 0);
}

#endif /* _REGION_H_ */
