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

#ifndef _MM_MMPROC_H_
#define _MM_MMPROC_H_

#include <asm/atomic.h>
#include <asm/page.h>
#include <lyos/avl.h>

struct mm_struct {
    atomic_t refcnt;

    vir_bytes vm_total;

    int slot;
    pgdir_t pgd;
    struct list_head mem_regions;
    struct avl_root mem_avl;
};

/* Memory related information of a process */
struct mmproc {
    int flags;

    struct mm_struct* mm;

    struct mmproc* group_leader;
    struct list_head group_list;

    endpoint_t endpoint;
};

#define MMPF_INUSE 0x01

#define mmproc2ep(mmp) ((mmp)-mmproc_table)

static inline void mmget(struct mm_struct* mm) { atomic_inc(&mm->refcnt); }
void mmput(struct mm_struct* mm);

#endif
