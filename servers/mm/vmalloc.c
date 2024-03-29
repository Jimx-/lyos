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

/**
 * Like alloc.c, however, vmalloc.c provides functions that allocate virtual
 * memory in kernel address space.
 *
 * alloc_vmem/free_vmem: allocate/free both physical and virtual memory.
 * alloc_vmpages/free_vmpages: allocate/free only virtual memory.
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "errno.h"
#include "lyos/const.h"
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include <asm/pagetable.h>

#include "proto.h"
#include "global.h"
#include "types.h"

#define NR_HOLES 2048

static struct hole hole[NR_HOLES]; /* the hole table */
static struct hole* hole_head;     /* pointer to first hole */
static struct hole* free_slots;    /* ptr to list of unused table slots */

static void delete_slot(struct hole* prev_ptr, struct hole* hp);
static void merge_hole(struct hole* hp);

void vmem_init(void* mem_start, size_t free_mem_size)
{
    struct hole* hp;

    /* Put all holes on the free list. */
    for (hp = &hole[0]; hp < &hole[NR_HOLES]; hp++) {
        hp->h_next = hp + 1;
        hp->h_base = 0;
        hp->h_len = 0;
    }
    hole[NR_HOLES - 1].h_next = NULL;
    hole_head = NULL;
    free_slots = &hole[0];

    /* Free memory */
    size_t nr_pages = free_mem_size >> ARCH_PG_SHIFT;
    if (free_mem_size % ARCH_PG_SIZE) nr_pages++;
    free_vmpages(mem_start, nr_pages);
}

/*****************************************************************************
 *                                alloc_vmem
 *****************************************************************************/
/**
 * Allocate a memory block for a proc.
 *
 * @param pid  Which proc the memory is for.
 * @param memsize  How many bytes is needed.
 *
 * @return  The base of the memory just allocated.
 *****************************************************************************/
void* alloc_vmem(phys_bytes* phys_addr, size_t memsize, int reason)
{
    /* avoid recursive allocation */
    static int level = 0;
    size_t pages = memsize / ARCH_PG_SIZE;
    if (memsize % ARCH_PG_SIZE != 0) pages++;

    level++;

    /* using bootstrap pages */
    if (level > 1 || !pt_init_done) {
        int i, j;
        for (i = 0; i < STATIC_BOOTSTRAP_PAGES; i++) {
            if (!bootstrap_pages[i].used) break;
        }

#ifdef __arm__
        /* allocate page directory at 16k alignment */
        if (reason == PGT_PAGEDIR) {
            while ((bootstrap_pages[i].phys_addr %
                        (sizeof(pde_t) * ARCH_VM_DIR_ENTRIES) !=
                    0) &&
                   i + pages < STATIC_BOOTSTRAP_PAGES) {
                bootstrap_pages[i].used = 1;
                i++;
            }
        }
#endif

        level--;
        if (i + pages >= STATIC_BOOTSTRAP_PAGES) {
            panic("out of bootstrap pages.");
            return 0;
        }

        if (phys_addr) *phys_addr = bootstrap_pages[i].phys_addr;
        void* ret = bootstrap_pages[i].vir_addr;
        for (j = i; j < i + pages; j++) {
            bootstrap_pages[j].used = 1;
        }

        return ret;
    }

    int memflags = 0;
#ifdef __arm__
    if (reason == PGT_PAGEDIR) {
        memflags |= APF_ALIGN16K;
    }
#endif

    /* allocate physical memory */
    phys_bytes phys_pages = alloc_pages(pages, memflags);
    if (phys_addr != NULL) *phys_addr = (phys_bytes)phys_pages;

    void* vir_pages = alloc_vmpages(pages);
    void* retval = vir_pages;

    pt_writemap(&mmproc_table[TASK_MM].mm->pgd, phys_pages,
                (vir_bytes)vir_pages, pages * ARCH_PG_SIZE, __P011);
    vmctl_flushtlb(SELF);

    level--;
    return retval;
}

/**
 * Allocate virtual memory pages.
 * @param  nr_pages How many pages are needed.
 * @return          Ptr to the memory.
 */
void* alloc_vmpages(size_t nr_pages)
{
    size_t memsize = nr_pages << ARCH_PG_SHIFT;
    struct hole *hp, *prev_ptr;
    vir_bytes old_base;

    prev_ptr = NULL;
    hp = hole_head;

    while (hp != NULL) {
        size_t alignment = 0;
        if (hp->h_base % ARCH_PG_SIZE != 0)
            alignment = ARCH_PG_SIZE - (hp->h_base % ARCH_PG_SIZE);
        if (hp->h_len >= memsize + alignment) {
            /* We found a hole that is big enough.  Use it. */
            old_base = hp->h_base + alignment;
            hp->h_base += memsize + alignment;
            hp->h_len -= memsize - alignment;
            if (prev_ptr && prev_ptr->h_base + prev_ptr->h_len == old_base)
                prev_ptr->h_len += alignment;

            /* Delete the hole if used up completely. */
            if (hp->h_len == 0) delete_slot(prev_ptr, hp);

            mem_info.vmalloc_used += memsize;
            /* Return the start address of the acquired block. */
            return (void*)old_base;
        }

        prev_ptr = hp;
        hp = hp->h_next;
    }

    panic("MM: alloc_vmpages() failed (out of virtual memory space).");

    return NULL;
}

void free_vmpages(void* base, size_t nr_pages)
{
    if (nr_pages <= 0) return;

    size_t len = nr_pages << ARCH_PG_SHIFT;
    struct hole *hp, *new_ptr, *prev_ptr;

    if ((new_ptr = free_slots) == NULL) panic("hole table full");
    new_ptr->h_base = (vir_bytes)base;
    new_ptr->h_len = len;
    free_slots = new_ptr->h_next;
    hp = hole_head;

    mem_info.vmalloc_used -= len;

    /* Insert the slot to a proper place */
    if (hp == NULL || (vir_bytes)base <= hp->h_base) {
        /* If there's no hole or the block's address is less than the lowest
           hole, put it on the front of the list */
        new_ptr->h_next = hp;
        hole_head = new_ptr;
        merge_hole(new_ptr);
        return;
    }

    /* Find where it should go */
    prev_ptr = NULL;
    while (hp != NULL && (vir_bytes)base > hp->h_base) {
        prev_ptr = hp;
        hp = hp->h_next;
    }

    new_ptr->h_next = prev_ptr->h_next;
    prev_ptr->h_next = new_ptr;
    merge_hole(prev_ptr);
}

/*****************************************************************************
 *                                free_vmem
 *****************************************************************************/
/**
 * Free a memory block.
 *
 * @param base	Base address of block to free.
 * @param len	Number of bytes to free.
 *
 * @return  Zero if success.
 *****************************************************************************/
void free_vmem(void* base, size_t len)
{
    if (!pt_init_done) return;

    size_t nr_pages = len >> ARCH_PG_SHIFT;
    if (len % ARCH_PG_SIZE) nr_pages++;

    /* free physical memory */
    int i, ret;
    vir_bytes addr = (vir_bytes)base;
    phys_bytes phys;
    struct mmproc* mmprocess = &mmproc_table[TASK_MM];
    for (i = 0; i < nr_pages; i++, addr += ARCH_PG_SIZE) {
        ret = pgd_va2pa(&mmprocess->mm->pgd, addr, &phys);
        if (!ret) free_mem(phys, ARCH_PG_SIZE);
    }

    free_vmpages(base, nr_pages);
}

/*******************************************************************
 *			delete_hole
 *******************************************************************/
/**
 * Remove an entry from the list.
 *
 *******************************************************************/
static void delete_slot(struct hole* prev_ptr, struct hole* hp)
{
    if (hp == hole_head)
        hole_head = hp->h_next;
    else
        prev_ptr->h_next = hp->h_next;

    hp->h_next = free_slots;
    hp->h_base = 0;
    hp->h_len = 0;
    free_slots = hp;
}

/*******************************************************************
 *			merge_hole
 *******************************************************************/
/**
 * Merge contiguous holes.
 *
 *******************************************************************/
static void merge_hole(struct hole* hp)
{
    struct hole* next_ptr;

    if ((next_ptr = hp->h_next) == NULL) return; /* last hole */
    if (hp->h_base + hp->h_len == next_ptr->h_base) {
        hp->h_len += next_ptr->h_len; /* first one gets second one's mem */
        delete_slot(hp, next_ptr);
    } else {
        hp = next_ptr;
    }

    if ((next_ptr = hp->h_next) == NULL) return; /* hp is the last hole now */
    if (hp->h_base + hp->h_len == next_ptr->h_base) {
        hp->h_len += next_ptr->h_len;
        delete_slot(hp, next_ptr);
    }
}
